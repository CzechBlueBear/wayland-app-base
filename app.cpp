#include "app.hpp"

//#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <cassert>
#include <cstring>

// versions of interfaces we want from the server
const uint32_t COMPOSITOR_API_VERSION = 4;
const uint32_t SHM_API_VERSION = 1;
const uint32_t SEAT_API_VERSION = 7;

static void randname(char *buf) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
        buf[i] = 'A'+(r&15)+(r&16)*2;
        r >>= 5;
    }
}

/**
 * Creates a new unsized, shared memory file and returns
 * a file descriptor to it.
 * The file is pre-unlinked and will be deleted as soon as
 * its file descriptor is closed.
 * Returns -1 on error.
 */
int WaylandApp::create_shm_file() {
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        randname(name + sizeof(name) - 7);
        --retries;
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);

    return -1;
}

/**
 * Allocates a new shared memory file of the given size and returns
 * a file descriptor to it.
 * Returns -1 on error.
 */
int WaylandApp::allocate_shm_file(size_t size)
{
    int fd = create_shm_file();
    if (fd < 0) {
        return -1;
    }

    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

WaylandApp* WaylandApp::the_app = nullptr;

/**
 * Returns a reference to the single existing instance of WaylandApp.
 */
WaylandApp &WaylandApp::the() {
    assert(the_app);
    return *the_app;
}

/**
 * Initializes the object but does not yet connect to Wayland server.
 * After creating, use connect() to establish a connection.
 */
WaylandApp::WaylandApp() {
    the_app = this;
}

WaylandApp::~WaylandApp() {
    disconnect();
    the_app = nullptr;
}

// coherent naming of listeners (to preserve our sanity):
// if structure is A_B_C_listener, then instance is A_B_C_listener_info,
// and callbacks are called WaylandApp::on_A_B_C_something().

static const struct wl_registry_listener wl_registry_listener_info = {
    .global = WaylandApp::on_registry_global,
    .global_remove = WaylandApp::on_registry_global_remove,
};

static const struct xdg_surface_listener xdg_surface_listener_info = {
    .configure = WaylandApp::on_xdg_surface_configure,
};

static const struct xdg_wm_base_listener xdg_wm_base_listener_info = {
    .ping = WaylandApp::on_xdg_wm_base_ping,
};

static const struct wl_buffer_listener wl_buffer_listener_info = {
    .release = WaylandApp::on_buffer_release,
};

static const xdg_toplevel_listener xdg_toplevel_listener_info = {
    .configure = WaylandApp::on_xdg_toplevel_configure,
    .close = WaylandApp::on_xdg_toplevel_close,
};

static const struct wl_pointer_listener pointer_listener = {
    .enter = WaylandApp::on_pointer_enter,
    .leave = WaylandApp::on_pointer_leave,
    .motion = WaylandApp::on_pointer_motion,
    .button = WaylandApp::on_pointer_button,
    .frame = WaylandApp::on_pointer_frame,
    //.axis = noop, // TODO
};

static const struct wl_seat_listener wl_seat_listener_info = {
    .capabilities = WaylandApp::on_seat_handle_capabilities,
    .name = WaylandApp::on_seat_name,
};

/**
 * Connects to the default Wayland server and creates all needed structures.
 * Returns true on success, false on failure.
 * Error messages are printed on stderr. No messages are produced if successful.
 * On failure, a best attempt is done to return to clean unconnected state
 * to allow another attempt.
 */
bool WaylandApp::connect() {
    assert(!m_display);
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        complain("wl_display_connect() failed, is the server running?");
        return false;
    }
    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        complain("wl_display_get_registry() failed");
        goto rollback;
    }

    wl_registry_add_listener(m_registry, &wl_registry_listener_info, nullptr);

    // during this roundtrip, the server should send us IDs of many globals,
    // including the compositor, SHM and XDG windowmanager base, and seat;
    // for each global, the on_registry_global() callback is called automatically
    wl_display_roundtrip(m_display);

    if (!m_compositor || !m_shm || !m_xdg_wm_base || !m_seat) {
        complain("missing one of interfaces: wl_compositor, wl_shm, wl_xdg_wm_base, wl_seat");
        goto rollback;
    }

    wl_seat_add_listener(m_seat, &wl_seat_listener_info, nullptr);
    xdg_wm_base_add_listener(m_xdg_wm_base, &xdg_wm_base_listener_info, nullptr);

    m_surface = wl_compositor_create_surface(m_compositor);
    if (!m_surface) {
        complain("wl_compositor_create_surface() failed");
        goto rollback;
    }

    m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface);
    if (!m_xdg_surface) {
        complain("xdg_wm_base_get_xdg_surface() failed");
        goto rollback;
    }

    xdg_surface_add_listener(m_xdg_surface, &xdg_surface_listener_info, nullptr);

    m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
    if (!m_xdg_toplevel) {
        complain("xdg_surface_get_toplevel() failed");
        goto rollback;
    }

    xdg_toplevel_set_title(m_xdg_toplevel, "Example client");
    xdg_toplevel_add_listener(m_xdg_toplevel, &xdg_toplevel_listener_info, nullptr);

    // render the first frame; without this, we won't get a visible window (why?)
    render_frame();

    return true;

rollback:
    if (m_xdg_surface) { xdg_surface_destroy(m_xdg_surface); m_xdg_surface = nullptr; }
    if (m_surface) { wl_surface_destroy(m_surface); m_surface = nullptr; }
    if (m_display) { wl_display_disconnect(m_display); m_display = nullptr; }
    return false;
}

/**
 * Disconnects from the Wayland server.
 * Attempts to cleanup and return to initial state so that a new connection
 * is possible if needed - even if the previous connection was only half finished.
 * Calling with no connection is safe and has no effect.
 */
void WaylandApp::disconnect() {

    // destroy objects we created (interfaces don't need destruction)
    if (m_xdg_toplevel) {
        xdg_toplevel_destroy(m_xdg_toplevel); m_xdg_toplevel = nullptr;
    }
    if (m_xdg_surface) {
        xdg_surface_destroy(m_xdg_surface); m_xdg_surface = nullptr;
    }
    if (m_surface) {
        wl_surface_destroy(m_surface); m_surface = nullptr;
    }

    // after all is cleaned up, disconnect
    if (m_display) {
        wl_display_disconnect(m_display); m_display = nullptr;
    }

    // clean up interface pointers
    m_registry = nullptr;
    m_compositor = nullptr;
    m_shm = nullptr;
    m_xdg_wm_base = nullptr;
    m_seat = nullptr;

    // reset the rest
    m_window_width = DEFAULT_WINDOW_WIDTH;
    m_window_height = DEFAULT_WINDOW_HEIGHT;
}

/**
 * Enters the event loop and proceeds handling events until the window is closed.
 */
void WaylandApp::enter_event_loop() {
    assert(m_display != nullptr);
    while (wl_display_dispatch(m_display) != -1 && !m_close_requested) {
        /* This space deliberately left blank */
    }
}

wl_buffer* WaylandApp::allocate_buffer(uint32_t width, uint32_t height) {
    int stride = width * 4;     // 4 bytes per pixel
    int size = stride * height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        complain("allocate_shm_file() failed");
        return nullptr;
    }

    uint32_t *data = (uint32_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        complain("mmap() failed for pixel buffer");
        close(fd);
        return nullptr;
    }

    assert(m_shm);
    wl_shm_pool *pool = wl_shm_create_pool(m_shm, fd, size);
    if (!pool) {
        complain("wl_shm_create_pool() failed");
        close(fd);
        return nullptr;
    }
    wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    if (!buffer) {
        complain("wl_shm_pool_create_buffer() failed");
        wl_shm_pool_destroy(pool);
        close(fd);
        return nullptr;
    }
    wl_shm_pool_destroy(pool);
    close(fd);

    draw(data, width, height);

    munmap(data, size);

    wl_buffer_add_listener(buffer, &wl_buffer_listener_info, nullptr);

    return buffer;
}

void WaylandApp::register_global(char const* interface, uint32_t name) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        m_compositor = (wl_compositor*)(wl_registry_bind(m_registry, name, &wl_compositor_interface, COMPOSITOR_API_VERSION));
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0) {
        m_shm = (wl_shm*)(wl_registry_bind(m_registry, name, &wl_shm_interface, SHM_API_VERSION));
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        m_xdg_wm_base = (xdg_wm_base*) wl_registry_bind(m_registry, name, &xdg_wm_base_interface, 1);
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
        m_seat = (wl_seat*) wl_registry_bind(m_registry, name, &wl_seat_interface, SEAT_API_VERSION);
    }
}

void WaylandApp::present_buffer(wl_buffer* buffer) {
    assert(m_surface);
    assert(buffer);
    wl_surface_attach(m_surface, buffer, 0, 0);
    wl_surface_commit(m_surface);
}

void WaylandApp::on_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

void WaylandApp::on_registry_global(void *data, wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version)
{
    the_app->register_global(interface, name);
}

void WaylandApp::on_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    // This space deliberately left blank
}

void WaylandApp::on_buffer_release(void* data, wl_buffer* buffer) {
    wl_buffer_destroy(buffer);
}

void WaylandApp::on_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    wl_buffer* buffer = the_app->allocate_buffer(the_app->m_window_width, the_app->m_window_height);
    if (!buffer) {
        return;
    }
    wl_surface_attach(the_app->m_surface, buffer, 0, 0);
    wl_surface_commit(the_app->m_surface);
}

void WaylandApp::on_xdg_toplevel_close(void* data, struct xdg_toplevel *xdg_toplevel) {
    the_app->m_close_requested = true;
}

void WaylandApp::on_xdg_toplevel_configure(void* data, struct xdg_toplevel* xdg_toplevel,
    int32_t width, int32_t height, struct wl_array *states)
{
    if (width != 0) { the_app->m_window_width = width; }
    if (height != 0) { the_app->m_window_height = height; }
    //xdg_surface_ack_configure(the_app->m_xdg_surface, serial);
}

void WaylandApp::on_seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        struct wl_pointer *pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(pointer, &pointer_listener, seat);
    }
}

void WaylandApp::on_pointer_button(void *data, struct wl_pointer *pointer,
        uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    struct wl_seat* seat = (wl_seat*) data;

/*
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
        xdg_toplevel_move(the_app->m_xdg_toplevel, seat, serial);
    }
*/
}

void WaylandApp::on_seat_name(void* data, struct wl_seat* seat, char const* name) {
    /* pass */
}

void WaylandApp::on_pointer_enter(void* data, struct wl_pointer* pointer, uint32_t, wl_surface*, wl_fixed_t x, wl_fixed_t y) {
    /* pass */
}

void WaylandApp::on_pointer_leave(void* data, struct wl_pointer* pointer, uint32_t, wl_surface*) {
    /* pass */
}

void WaylandApp::on_pointer_motion(void *data, struct wl_pointer *wl_pointer,
               uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    /* pass */
}

void WaylandApp::on_pointer_frame(void* data, struct wl_pointer* pointer) {
    /* pass */
}

void WaylandApp::render_frame() {
    assert(m_surface);
    wl_buffer* buffer = allocate_buffer(m_window_width, m_window_height);
    if (!buffer) {
        return;
    }
    wl_surface_attach(m_surface, buffer, 0, 0);
    wl_surface_commit(m_surface);
}

void WaylandApp::complain(char const* message) {
    fprintf(stderr, "error: wayland: %s\n", message);
}

void WaylandApp::draw(uint32_t* pixels, int width, int height) {

    /* Draw checkerboxed background */
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if ((x + y / 8 * 8) % 16 < 8)
                pixels[y * width + x] = 0xFF666666;
            else
                pixels[y * width + x] = 0xFFEEEEEE;
        }
    }
}