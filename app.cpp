#include "app.hpp"

//#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <cassert>
#include <cstring>

const uint32_t COMPOSITOR_API_VERSION = 4;
const uint32_t SHM_API_VERSION = 1;

static void randname(char *buf) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
        buf[i] = 'A'+(r&15)+(r&16)*2;
        r >>= 5;
    }
}

static int create_shm_file() {
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

static int allocate_shm_file(size_t size)
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
    the_app = nullptr;
    disconnect();
}

static void noop() {
    /* do nothing */
}

static const struct wl_registry_listener wl_registry_listener_info = {
    .global = WaylandApp::on_registry_handle_global,
    .global_remove = WaylandApp::on_registry_handle_global_remove,
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
    .close = WaylandApp::on_xdg_toplevel_handle_close,
};

static const struct wl_pointer_listener pointer_listener = {
    //.enter = noop,
    //.leave = noop,
    //.motion = noop,
    .button = WaylandApp::on_pointer_handle_button,
    //.axis = noop,
};

static const struct wl_seat_listener seat_listener = {
    .capabilities = WaylandApp::on_seat_handle_capabilities,
};

/**
 * Connects to the default Wayland server and creates all needed structures.
 * Returns true on success, false on failure.
 * Error messages are printed on stderr. No messages are produced if successful.
 * On failure, a best attempt is done to return to clean unconnected state to allow another attempt.
 */
bool WaylandApp::connect() {
    if (m_display != nullptr) {
        return true;
    }
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        complain("wl_display_connect() failed, is the server running?");
        return false;
    }
    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        complain("wl_display_get_registry() failed");
        wl_display_disconnect(m_display); m_display = nullptr;
        return false;
    }

    wl_registry_add_listener(m_registry, &wl_registry_listener_info, nullptr);

    // during this roundtrip, the server should send us IDs of many globals,
    // including the compositor, SHM and XDG windowmanager base
    wl_display_roundtrip(m_display);

    if (!m_compositor || !m_shm || !m_xdg_wm_base) {
        complain("missing one of interfaces: wl_compositor, wl_shm, wl_xdg_wm_base");
        wl_display_disconnect(m_display); m_display = nullptr;
        return false;
    }

    xdg_wm_base_add_listener(m_xdg_wm_base, &xdg_wm_base_listener_info, nullptr);

    m_surface = wl_compositor_create_surface(m_compositor);
    if (!m_surface) {
        complain("wl_compositor_create_surface() failed");
        wl_display_disconnect(m_display); m_display = nullptr;
        return false;
    }

    m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface);
    if (!m_xdg_surface) {
        complain("xdg_wm_base_get_xdg_surface() failed");
        wl_surface_destroy(m_surface); m_surface = nullptr;
        wl_display_disconnect(m_display); m_display = nullptr;
        m_display = nullptr;
        return false;
    }

    xdg_surface_add_listener(m_xdg_surface, &xdg_surface_listener_info, nullptr);

    m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
    if (!m_xdg_toplevel) {
        complain("xdg_surface_get_toplevel() failed");
        xdg_surface_destroy(m_xdg_surface); m_xdg_surface = nullptr;
        wl_surface_destroy(m_surface); m_surface = nullptr;
        wl_display_disconnect(m_display); m_display = nullptr;
        return false;
    }

    xdg_toplevel_set_title(m_xdg_toplevel, "Example client");
    xdg_toplevel_add_listener(m_xdg_toplevel, &xdg_toplevel_listener_info, nullptr);

    // render the first frame; without this, we won't get a visible window
    render_frame();

    return true;
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

    // reset the rest
    m_globals.clear();
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

void WaylandApp::bind_to_compositor(uint32_t name) {
    assert(m_registry);
    m_compositor = (wl_compositor*)(wl_registry_bind(m_registry, name, &wl_compositor_interface, COMPOSITOR_API_VERSION));
    if (!m_compositor) {
        complain("wl_registry_bind() failed for wl_compositor");
    }
}

void WaylandApp::bind_to_shm(uint32_t name) {
    assert(m_registry);
    m_shm = (wl_shm*)(wl_registry_bind(m_registry, name, &wl_shm_interface, SHM_API_VERSION));
    if (!m_shm) {
        complain("wl_registry_bind() failed for wl_shm");
    }
}

void WaylandApp::bind_to_xdg_wm_base(uint32_t name) {
    assert(m_registry);
    m_xdg_wm_base = (xdg_wm_base*) wl_registry_bind(m_registry, name, &xdg_wm_base_interface, 1);
    if (!m_xdg_wm_base) {
        complain("wm_registry_bind() failed for xdg_wm_base");
    }
}

wl_buffer* WaylandApp::allocate_buffer(uint32_t width, uint32_t height) {
    int stride = width * 4;
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

    /* Draw checkerboxed background */
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if ((x + y / 8 * 8) % 16 < 8)
                data[y * width + x] = 0xFF666666;
            else
                data[y * width + x] = 0xFFEEEEEE;
        }
    }

    munmap(data, size);

    wl_buffer_add_listener(buffer, &wl_buffer_listener_info, nullptr);

    return buffer;
}

void WaylandApp::register_global(char const* interface, uint32_t name) {
    m_globals[interface] = name;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        printf("compositor interface obtained (name=%d, id=%s)\n", name, wl_compositor_interface.name);
        bind_to_compositor(name);
    }
    else if (strcmp(interface, wl_shm_interface.name) == 0) {
        printf("shm interface obtained (name=%d, id=%s)\n", name, wl_shm_interface.name);
        bind_to_shm(name);
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        printf("xdg_wm_base interface obtained (name=%d, id=%s)\n", name, xdg_wm_base_interface.name);
        bind_to_xdg_wm_base(name);
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

void WaylandApp::on_registry_handle_global(void *data, wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version)
{
    the_app->register_global(interface, name);
}

void WaylandApp::on_registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
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

void WaylandApp::on_xdg_toplevel_handle_close(void* data, struct xdg_toplevel *xdg_toplevel) {
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

void WaylandApp::on_pointer_handle_button(void *data, struct wl_pointer *pointer,
        uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    struct wl_seat* seat = (wl_seat*) data;

/*
    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
        xdg_toplevel_move(the_app->m_xdg_toplevel, seat, serial);
    }
*/
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
