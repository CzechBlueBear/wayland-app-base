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
    assert(the_app == nullptr);
    the_app = this;
}

WaylandApp::~WaylandApp() {
    assert(the_app == this);
    the_app = nullptr;
    disconnect();
}

static const struct wl_registry_listener wl_registry_listener_info = {
    .global = WaylandApp::on_registry_handle_global,
    .global_remove = WaylandApp::on_registry_handle_global_remove,
};

/**
 * Connects to the default Wayland server and creates all needed structures.
 * Returns true on success, false on failure.
 * Error messages are printed on stderr. No messages are produced if successful.
 */
bool WaylandApp::connect() {
    if (m_display != nullptr) {
        return true;
    }
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        fprintf(stderr, "error: could not connect to Wayland display\n");
        return false;
    }
    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        fprintf(stderr, "error: wl_display_get_registry() failed\n");
        wl_display_disconnect(m_display);
        m_display = nullptr;
        return false;
    }

    wl_registry_add_listener(m_registry, &wl_registry_listener_info, nullptr);

    // during this roundtrip, the server should send us IDs of many globals,
    // including the compositor, SHM and XDG windowmanager base
    wl_display_roundtrip(m_display);

    if (!m_compositor || !m_shm || !m_xdg_wm_base) {
        fprintf(stderr, "error: missing one of globals (wl_compositor, wl_shm, wl_xdg_wm_base)\n");
        wl_display_disconnect(m_display);
        m_display = nullptr;
        return false;
    }

    m_surface = wl_compositor_create_surface(m_compositor);
    m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface);
    m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface);

    xdg_toplevel_set_title(m_xdg_toplevel, "Example client");

    return true;
}

void WaylandApp::disconnect() {
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
        m_registry = nullptr;
        m_compositor = nullptr;
        m_shm = nullptr;
        m_xdg_wm_base = nullptr;
        m_xdg_surface = nullptr;
        m_xdg_toplevel = nullptr;
    }
}

void WaylandApp::handle_events() {
    assert(m_display != nullptr);
    while (wl_display_dispatch(m_display) != -1) {
        /* This space deliberately left blank */
    }
}

void WaylandApp::bind_to_compositor(uint32_t name) {
    assert(m_registry);
    m_compositor = (wl_compositor*)(wl_registry_bind(m_registry, name, &wl_compositor_interface, COMPOSITOR_API_VERSION));
    if (!m_compositor) {
        fprintf(stderr, "error: wl_registry_bind() failed\n");
    }
}

void WaylandApp::bind_to_shm(uint32_t name) {
    assert(m_registry);
    m_shm = (wl_shm*)(wl_registry_bind(m_registry, name, &wl_shm_interface, SHM_API_VERSION));

    if (!m_shm) {
        fprintf(stderr, "error: wl_registry_bind() failed\n");
        return;
    }

/*
    m_surface = wl_compositor_create_surface(m_compositor);
    if (!m_surface) {
        fprintf(stderr, "error: wl_compositor_create_surface() failed\n");
        return;
    }
*/
}

static const struct xdg_wm_base_listener xdg_wm_base_listener_info = {
    .ping = WaylandApp::on_xdg_wm_base_ping,
};

void WaylandApp::bind_to_xdg_wm_base(uint32_t name) {
    assert(m_registry);
    m_xdg_wm_base = (xdg_wm_base*) wl_registry_bind(m_registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(m_xdg_wm_base, &xdg_wm_base_listener_info, nullptr);
}

static const struct wl_buffer_listener wl_buffer_listener_info = {
    .release = WaylandApp::on_buffer_release,
};

wl_buffer* WaylandApp::allocate_buffer(uint32_t width, uint32_t height) {
    int stride = width * 4;
    int size = stride * height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        return nullptr;
    }

    uint32_t *data = (uint32_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return nullptr;
    }

    assert(m_shm);
    wl_shm_pool *pool = wl_shm_create_pool(m_shm, fd, size);
    wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
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

void WaylandApp::present_buffer(wl_buffer* buffer) {
    assert(m_surface != nullptr);
    assert(buffer != nullptr);
    wl_surface_attach(m_surface, buffer, 0, 0);
    wl_surface_commit(m_surface);
}

void WaylandApp::on_buffer_release(void* data, wl_buffer* buffer) {
    wl_buffer_destroy(buffer);
}
