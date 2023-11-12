#include "app.hpp"
#include "shm_util.hpp"
#include "debug.hpp"
#include "xdg-shell-client-protocol.h"

//#define _POSIX_C_SOURCE 200112L
#include <cassert>
#include <cerrno>
#include <cstring>
#include <sys/mman.h>

wl::Connection::Connection() {
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        complain("could not connect to Wayland server: " + errno_to_string());
        return;
    }

    m_listener.error = [](void* self_, wl_display* display, void*, uint32_t code, const char* message){
        complain("Wayland client error: " + std::string(message));
    };

    wl_display_add_listener(m_display, &m_listener, this);
}

wl::Connection::~Connection() {
    if (m_display) {
        wl_display_disconnect(m_display);
    }
}

void wl::Connection::roundtrip() {
    assert(m_display);
    wl_display_roundtrip(m_display);
}

int wl::Connection::dispatch() {
    assert(m_display);
    return wl_display_dispatch(m_display);
}

wl::Registry::Registry(wl::Connection& conn) {
    m_registry = wl_display_get_registry(*conn);
    if (!m_registry) {
        complain("could not bind to Wayland registry");
    }

    static const wl_registry_listener listener {
        .global = [](void* self_, wl_registry* registry, uint32_t name, const char* iface, uint32_t version) {
            auto self = (wl::Registry*)self_;
            self->m_interfaces[iface] = name;
        },
        .global_remove = [](void* self_, wl_registry* registry, uint32_t name) {
            // todo
        }
    };
    wl_registry_add_listener(m_registry, &listener, this);
}

wl::Registry::~Registry() {
    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
}

wl::Compositor::Compositor(Registry& registry) {
    m_compositor = registry.bind<wl_compositor>(&wl_compositor_interface, API_VERSION);
}

wl::Compositor::~Compositor() {
    if (m_compositor) {
        wl_compositor_destroy(m_compositor);
    }
}

wl::Shm::Shm(Registry& registry) {
    m_shm = registry.bind<wl_shm>(&wl_shm_interface, API_VERSION);
}

wl::Shm::~Shm() {
    if (m_shm) {
        wl_shm_destroy(m_shm);
    }
}

wl::Seat::Seat(Registry& registry) {
    m_seat = registry.bind<wl_seat>(&wl_seat_interface, API_VERSION);
    if (!m_seat) {
        return;
    }

    m_listener.capabilities = [](void* self_, wl_seat* seat, uint32_t caps) {
        auto self = (wl::Seat*)self_;

        // capabilities can both appear and disappear
        self->m_pointer_supported = false;
        self->m_keyboard_supported = false;
        self->m_touch_supported = false;

        // look what is supported right now
        if (caps & WL_SEAT_CAPABILITY_POINTER) {
            self->m_pointer_supported = true;
        }
        if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
            self->m_keyboard_supported = true;
        }
        if (caps & WL_SEAT_CAPABILITY_TOUCH) {
            self->m_touch_supported = true;
        }
    };
    m_listener.name = [](void* self_, wl_seat* seat, const char* name) {
        auto self = (wl::Seat*)self_;
        self->m_name = name;
    };

    wl_seat_add_listener(m_seat, &m_listener, this);
}

wl::Seat::~Seat() {
    if (m_seat) {
        wl_seat_destroy(m_seat);
    }
}

xdg::wm::Base::Base(wl::Registry& registry) {
    m_base = registry.bind<xdg_wm_base>(&xdg_wm_base_interface, API_VERSION);

    m_listener.ping = [](void* self_, xdg_wm_base* base, uint32_t serial) {
        auto self = (xdg::wm::Base*)self_;
        self->pong(serial);
    };

    xdg_wm_base_add_listener(m_base, &m_listener, this);
}

xdg::wm::Base::~Base() {
    if (m_base) {
        xdg_wm_base_destroy(m_base);
    }
}

void xdg::wm::Base::pong(uint32_t serial_number) {
    xdg_wm_base_pong(m_base, serial_number);
}

wl::Surface::Surface(wl::Compositor& compositor) {
    m_surface = wl_compositor_create_surface(compositor.get());
    if (!m_surface) {
        complain("wl_compositor_create_surface() failed");
    }
}

wl::Surface::~Surface() {
    if (m_surface) {
        wl_surface_destroy(m_surface);
    }
}

void wl::Surface::commit() {
    assert(m_surface);
    wl_surface_commit(m_surface);
}

xdg::Surface::Surface(xdg::wm::Base& base, wl::Surface& low_surface) {
    m_surface = xdg_wm_base_get_xdg_surface(base.get(), low_surface.get());
    if (!m_surface) {
        complain("xdg_wm_base_get_xdg_surface() failed");\
        return;
    }

    m_listener.configure = [](void* self_, xdg_surface* surface, uint32_t serial) {
        auto self = (xdg::Surface*)self_;
        self->m_last_configure_event_serial = serial;
        self->m_configure_event_pending = true;
    };

    xdg_surface_add_listener(m_surface, &m_listener, this);
}

xdg::Surface::~Surface() {
    if (m_surface) {
        xdg_surface_destroy(m_surface);
    }
}

void xdg::Surface::ack_configure() {
    if (!m_configure_event_pending) {
        complain("no configure event is pending");
        return;
    }
    xdg_surface_ack_configure(m_surface, m_last_configure_event_serial);
    m_configure_event_pending = false;
}

void xdg::Surface::set_window_geometry(int32_t x, int32_t y, int32_t width, int32_t height) {
    assert(m_surface);
    xdg_surface_set_window_geometry(m_surface, x, y, width, height);
}

xdg::Toplevel::Toplevel(xdg::Surface& surface) {
    m_toplevel = xdg_surface_get_toplevel(surface.get());
    if (!m_toplevel) {
        complain("xdg_surface_get_toplevel() failed");
        return;
    }

    m_listener.configure = [](void* self_, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states) {
        auto self = (xdg::Toplevel*) self_;
        self->m_last_requested_width = width;
        self->m_last_requested_height = height;
        self->m_configure_requested = true;
        info("configure request received: " + std::to_string(width) + "x" + std::to_string(height));
        // TODO: we should ack this, but how when we don't know the serial number?
    };
    m_listener.close = [](void* self_, xdg_toplevel* toplevel) {
        auto self = (xdg::Toplevel*) self_;
        self->m_close_requested = true;
    };
    m_listener.configure_bounds = [](void* self_, xdg_toplevel* toplevel, int32_t width, int32_t height) {
        auto self = (xdg::Toplevel*) self_;
        self->m_recommended_max_width = width;
        self->m_recommended_max_height = height;
        info("received recommended max dimensions: " + std::to_string(width) + "x" + std::to_string(height));
    };

    xdg_toplevel_add_listener(m_toplevel, &m_listener, this);
}

xdg::Toplevel::~Toplevel() {
    xdg_toplevel_destroy(m_toplevel);
}

void xdg::Toplevel::set_title(std::string title) {
    xdg_toplevel_set_title(m_toplevel, title.c_str());
}

xdg::DecorationManager::DecorationManager(wl::Registry& registry) {
    m_manager = registry.bind<zxdg_decoration_manager_v1>(&zxdg_decoration_manager_v1_interface, API_VERSION);
}

xdg::DecorationManager::~DecorationManager() {
    zxdg_decoration_manager_v1_destroy(m_manager);
}

xdg::ToplevelDecoration::ToplevelDecoration(xdg::DecorationManager& manager, xdg::Toplevel& toplevel) {
    m_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(manager.get(), toplevel.get());
}

xdg::ToplevelDecoration::~ToplevelDecoration() {
    if (m_decoration) {
        zxdg_toplevel_decoration_v1_destroy(m_decoration);
    }
}

void xdg::ToplevelDecoration::set_server_side_mode() {
    assert(m_decoration);
    zxdg_toplevel_decoration_v1_set_mode(m_decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

wayland::Display::Display() {
    m_connection= std::make_unique<wl::Connection>();
    if (!m_connection) { return; }
    m_registry = std::make_unique<wl::Registry>(*m_connection);
    if (!m_registry) { return; }

    // during this roundtrip, the server should send us IDs of many globals,
    // including the compositor, SHM and XDG windowmanager base, and seat
    m_connection->roundtrip();

    m_compositor = std::make_unique<wl::Compositor>(*m_registry);
    if (!m_compositor) { return; }
    m_shm = std::make_unique<wl::Shm>(*m_registry);
    if (!m_shm) { return; }
    m_seat = std::make_unique<wl::Seat>(*m_registry);
    if (!m_seat) { return; }
    m_wm_base = std::make_unique<xdg::wm::Base>(*m_registry);
    if (!m_wm_base) { return; }

    // if we got here, we are good
    m_good = true;
}

wayland::Window::Window(wayland::Display& display) {
    m_surface = std::make_unique<wl::Surface>(display.get_compositor());
    if (!m_surface->is_good()) { return; }
    m_xdg_surface = std::make_unique<xdg::Surface>(display.get_wm_base(), *m_surface);
    if (!m_xdg_surface->is_good()) { return; }
    m_toplevel = std::make_unique<xdg::Toplevel>(*m_xdg_surface);
    if (!m_toplevel->is_good()) { return; }
    m_good = true;
}

wayland::Frame::Frame(wayland::Display& display, int32_t width, int32_t height) {
    assert(width >= 0 && height >= 0);

    int32_t size = width*height*4;  // 4 bytes per pixel (XRGB or ARGB)

    // an anonymous in-memory file to share with the Wayland server
    int fd = memfd_create("frame", MFD_CLOEXEC|MFD_ALLOW_SEALING);
    if (!fd) {
        complain("frame: memfd_create() failed: " + errno_to_string());
        return;
    }
    for(;;) {
        int ret = ftruncate(fd, size);
        if (ret == 0) { break; }
        if (errno != EINTR) {
            complain("frame: ftruncate() failed: " + errno_to_string());
            close(fd);
            return;
        }
    }
    void* memory = mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (memory == MAP_FAILED) {
        complain("frame: mmap() failed: " + errno_to_string());
        close(fd);
        return;
    }

    // temporary pool to allocate the frame from (deleted at the end of this call)
    std::unique_ptr<wl_shm_pool, wl_shm_pool_deleter> pool(
        wl_shm_create_pool(display.get_shm().get(), fd, size));
    if (!pool) {
        complain("frame: wl_shm_create_pool() failed: " + errno_to_string());
        munmap(memory, size);
        close(fd);
        return;
    }

    int32_t stride = width*4;

    // allocate the given size of the buffer from the pool
    std::unique_ptr<wl_buffer, wl_buffer_deleter> buffer {
        wl_shm_pool_create_buffer(pool.get(), 0, width, height, stride, WL_SHM_FORMAT_XRGB8888)
    };
    if (!buffer) {
        complain("frame: wl_shm_pool_create_buffer() failed: " + errno_to_string());
        munmap(memory, size);
        close(fd);
        return;
    }

    // close filedescriptor to conserve them; the mapping stays
    close(fd);

    // install a listener to inform us when the Wayland server releases the buffer
    m_listener.release = [](void* self_, wl_buffer* buffer) {
        auto self = (wayland::Frame*) self_;
        delete self;
        //self->m_attached = false;
    };
    wl_buffer_add_listener(buffer.get(), &m_listener, this);

    m_memory = memory;
    m_size = size;
    m_width = width;
    m_height = height;
    m_buffer = std::move(buffer);
    m_good = true;
}

wayland::Frame::~Frame() {
    munmap(m_memory, m_size);
}

void wayland::Frame::attach(wayland::Window& window) {
    assert(m_good);
    assert(!m_attached);
    m_attached = true;
    wl_surface_attach(window.get_surface().get(), m_buffer.get(), 0, 0);
}

WaylandApp* WaylandApp::the_app = nullptr;

/**
 * Returns a reference to the single existing instance of WaylandApp.
 */
WaylandApp &WaylandApp::the() {
    assert(the_app);
    return *the_app;
}

WaylandApp::~WaylandApp() {
    the_app = nullptr;
}

WaylandApp::WaylandApp() {
    assert(!the_app);
    the_app = this;

    m_display = std::make_unique<wayland::Display>();
    if (!m_display->is_good()) {
        complain("display initialization failed");
        return;
    }

    m_window = std::make_unique<wayland::Window>(*m_display);
    if (!m_window->is_good()) {
        complain("window initialization failed");
        return;
    }
}

/**
 * Enters the event loop and proceeds handling events until the window is closed.
 */
void WaylandApp::enter_event_loop() {
    info("event loop entered");

    assert(m_display);

    int redraws = 0;
    int revolutions = 0;

    int32_t wanted_width = DEFAULT_WINDOW_WIDTH;
    int32_t wanted_height = DEFAULT_WINDOW_HEIGHT;

    // first render
    auto frame = new wayland::Frame(*m_display, wanted_width, wanted_height);
    render_frame(*frame);
    frame->attach(*m_window);

    bool need_redraw = true;
    while (m_display->get_connection().dispatch() != -1) {
        revolutions++;

        if (m_window->get_xdg_surface().is_configure_event_pending()) {
            wanted_width = m_window->get_toplevel().get_last_requested_width();
            wanted_height = m_window->get_toplevel().get_last_requested_height();
            if (wanted_width == 0) {
                wanted_width = DEFAULT_WINDOW_WIDTH;
            }
            if (wanted_height == 0) {
                wanted_height = DEFAULT_WINDOW_HEIGHT;
            }
            m_window->get_xdg_surface().ack_configure();
            need_redraw = true;
        }

        if (need_redraw) {
            auto frame = new wayland::Frame(*m_display, wanted_width, wanted_height);
            render_frame(*frame);
            frame->attach(*m_window);
            m_window->get_surface().commit();
            redraws++;
        }
        fprintf(stdout, "wayland app running, %d redraws, %d event revs\r", redraws, revolutions);

        if (m_window->get_toplevel().is_close_requested()) {
            break;
        }
    }
}

void WaylandApp::render_frame(wayland::Frame& frame) {
    DrawingContext dc = DrawingContext(
        (uint32_t*) frame.get_memory(),
        frame.get_width(),
        frame.get_height());
    draw(dc);
}

void WaylandApp::draw(DrawingContext ctx) {

    // color transition from green to blue
    for(int y = 0; y < ctx.height(); ++y) {
        float height_fraction = (float)y/(float)ctx.height();
        ctx.xline(0, y, ctx.width(),
            0xFF000000 |
            (int)((1.0f - height_fraction)*255.0f) << 8 | 
            (int)(height_fraction * 255.0f)
        );
    }

    // central white rectangle
    ctx.draw_rect(64, 64, ctx.width()-128, ctx.height()-128, 0xFFFFFFFF);
    ctx.fill_rect(65, 65, ctx.width()-130, ctx.height()-130, 0xFFDDDDDD);

#if 0
    /* Draw checkerboxed background */
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if ((x + y / 8 * 8) % 16 < 8)
                pixels[y * width + x] = 0xFF666666;
            else
                pixels[y * width + x] = 0xFFEEEEEE;
        }
    }
#endif
}
