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

WaylandApp* the_app = nullptr;

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

static void
callback_registry_handle_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version)
{
    the_app->register_global(interface, name);
}

static void
callback_registry_handle_global_remove(void *data, struct wl_registry *registry,
        uint32_t name)
{
    // This space deliberately left blank
}

static const struct wl_registry_listener
registry_listener = {
    .global = callback_registry_handle_global,
    .global_remove = callback_registry_handle_global_remove,
};

static void
callback_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = callback_xdg_wm_base_ping,
};


WaylandApp::WaylandApp() {
    the_app = this;
}

/**
 * Connects to the default Wayland server and creates all needed structures.
 * Returns true on success, false on failure.
 * Calling connect() while already connected is safe, does nothing, and returns true.
 * On failure, the object stays in well-defined disconnected state and retry is possible.
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
        fprintf(stderr, "error: could not get display registry\n");
        wl_display_disconnect(m_display);
        return false;
    }
    wl_registry_add_listener(m_registry, &registry_listener, nullptr);
    wl_display_roundtrip(m_display);
    return true;
}

void WaylandApp::disconnect() {
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
        m_registry = nullptr;
        m_compositor = nullptr;
    }
}

void WaylandApp::handle_events() {
    assert(m_display != nullptr);
    while (wl_display_dispatch(m_display) != -1) {
        /* This space deliberately left blank */
    }
}

void WaylandApp::bind_to_compositor(uint32_t name) {
    assert(m_compositor == nullptr);
    m_compositor = (wl_compositor*)(
        wl_registry_bind(
            m_registry,
            name,
            &wl_compositor_interface,
            COMPOSITOR_API_VERSION)
    );
    if (!m_compositor) {
        fprintf(stderr, "error: wl_registry_bind() failed for Wayland compositor\n");
    }
}

void WaylandApp::bind_to_shm(uint32_t name) {
    assert(m_shm == nullptr);
    m_shm = (wl_shm*)(
        wl_registry_bind(
            m_registry,
            name,
            &wl_shm_interface,
            SHM_API_VERSION)
    );
    if (!m_shm) {
        fprintf(stderr, "error: wl_registry_bind() failed for Wayland SHM interface\n");
        return;
    }

    m_surface = wl_compositor_create_surface(m_compositor);
    if (!m_surface) {
        fprintf(stderr, "error: wl_compositor_create_surface() failed\n");
        return;
    }
}

void WaylandApp::bind_to_xdg_wm_base(uint32_t name) {
    static const struct xdg_wm_base_listener listener_info = {
        .ping = WaylandApp::callback_xdg_wm_base_ping,
    };

    m_xdg_wm_base = (xdg_wm_base*) wl_registry_bind(
        m_registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(m_xdg_wm_base,
        &xdg_wm_base_listener, nullptr);
}

bool WaylandApp::create_buffer_pool(uint32_t width, uint32_t height) {
    if (!m_shm) {
        fprintf(stderr, "error: would create SHM pool but have no SHM interface\n");
        return false;
    }

    m_pool_stride = width * 4;
    m_shm_pool_size = height * m_pool_stride * 2;  // *2 for double buffering

    int fd = allocate_shm_file(m_shm_pool_size);
    if (fd < 0) {
        fprintf(stderr, "error: could not allocate SHM file for Wayland suface\n");
        return false;
    }

    assert(m_pool_data == nullptr);
    m_pool_data = (uint8_t*) mmap(NULL, m_shm_pool_size,
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m_pool_data == MAP_FAILED) {
        fprintf(stderr, "error: mmap() failed for Wayland SHM surface\n");
        m_pool_data = nullptr;
        return false;
    }

    m_shm_pool = wl_shm_create_pool(m_shm, fd, m_shm_pool_size);
    if (!m_shm_pool) {
        fprintf(stderr, "error: wl_shm_create_pool() failed\n");
        munmap(m_pool_data, m_shm_pool_size);
        m_pool_data = nullptr;
        return false;
    }

    if (!create_buffer(0, width, height)) {
        fprintf(stderr, "error: could not create buffer#0\n");
        return false;
    }
    if (!create_buffer(1, width, height)) {
        fprintf(stderr, "error: could not create buffer#1\n");
        return false;
    }

    return true;
}

bool WaylandApp::create_buffer(int index, uint32_t width, uint32_t height) {
    int offset = height * m_pool_stride * index;
    m_buffers[index] = wl_shm_pool_create_buffer(m_shm_pool, offset,
        width, height, m_pool_stride, WL_SHM_FORMAT_XRGB8888);
    if (m_buffers[index] == nullptr) {
        fprintf(stderr, "error: wl_shm_pool_create_buffer() failed\n");
        return false;
    }

    return true;
}

void WaylandApp::attach_and_redraw_buffer(int buffer_index) {
    assert(m_surface != nullptr);
    wl_surface_attach(m_surface, m_buffers[buffer_index], 0, 0);
    wl_surface_damage(m_surface, 0, 0, UINT32_MAX, UINT32_MAX);
    wl_surface_commit(m_surface);
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

void WaylandApp::callback_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

void WaylandApp::callback_registry_handle_global(void *data, wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version)
{
    the_app->register_global(interface, name);
}
