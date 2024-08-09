#include "simple_app.hpp"
#include "debug.hpp"
#include <stdexcept>

void wayland::Connection::connect() {
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        throw std::runtime_error("wl_display_connect() failed");
    }

    m_display_listener.error = [](void* self_, wl_display* display, void*, uint32_t code, const char* message) {
        complain("Wayland client error: " + std::string(message));
    };

    wl_display_add_listener(m_display, &m_display_listener, this);

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        reset();
        throw std::runtime_error("wl_display_get_registry() failed");
    }

    m_registry_listener.global = [](void* self_, wl_registry* registry, uint32_t name, const char* iface, uint32_t version) {
        auto self = (wayland::Connection*)self_;
        self->m_interfaces[iface] = name;
    };
    m_registry_listener.global_remove = [](void* self_, wl_registry* registry, uint32_t name) {
        // todo
    };
    wl_registry_add_listener(m_registry, &m_registry_listener, this);

    // the first roundtrip is the point where the server sends
    // IDs of basic interfaces which we then bound to
    roundtrip();

    m_compositor = bind_interface<wl_compositor>(&wl_compositor_interface, COMPOSITOR_API_VERSION);
    if (!m_compositor) {
        reset();
        throw std::runtime_error("Wayland compositor interface not found");
    }
    m_shm = bind_interface<wl_shm>(&wl_shm_interface, SHM_API_VERSION);
    if (!m_shm) {
        reset();
        throw std::runtime_error("Wayland SHM interface not found");
    }
    m_seat = bind_interface<wl_seat>(&wl_seat_interface, SEAT_API_VERSION);
    if (!m_seat) {
        reset();
        throw std::runtime_error("Wayland seat interface not found");
    }

    m_seat_listener.capabilities = [](void* self_, wl_seat* seat, uint32_t caps) {
        auto self = (wayland::Connection*)self_;

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
    m_seat_listener.name = [](void* self_, wl_seat* seat, const char* name) {
        auto self = (wayland::Connection*)self_;
        self->m_seat_name = name;
    };

    wl_seat_add_listener(m_seat, &m_seat_listener, this);

    m_wm_base = bind_interface<xdg_wm_base>(&m_wm_base, WM_BASE_API_VERSION);
    if (!m_wm_base) {
        reset();
        throw std::runtime_error("Wayland window manager base interface not found");
    }

    m_wm_base_listener.ping = [](void* self_, xdg_wm_base* base, uint32_t serial) {
        auto self = (wayland::Connection*)self_;
        self->pong(serial);
    };

    xdg_wm_base_add_listener(m_wm_base, &m_wm_base_listener, this);

    m_surface = wl_compositor_create_surface(m_compositor);
    if (!m_surface) {
        reset();
        throw std::runtime_error("Could not create Wayland surface");
    }

    m_xdg_surface = xdg_wm_base_get_xdg_surface(m_wm_base, m_surface);
    if (!m_xdg_surface) {
        reset();
        throw std::runtime_error("Could not create Wayland XDG surface");
    }

    m_xdg_surface_listener.configure = [](void* self_, xdg_surface* surface, uint32_t serial) {
        auto self = (wayland::Connection*)self_;
        self->m_last_configure_event_serial = serial;
        self->m_configure_event_pending = true;
    };

    xdg_surface_add_listener(m_surface, &m_xdg_surface_listener, this);

    m_window = wl_egl_window_create(m_surface, m_window_width, m_window_height);
    if (!m_window) {
        reset();
        throw std::runtime_error("Could not create EGL window");
    }

    m_toplevel = xdg_surface_get_toplevel(m_xdg_surface);
    if (!m_toplevel) {
        reset();
        throw std::runtime_error("Could not get a Wayland toplevel object for surface");
    }

    m_toplevel_listener.configure = [](void* self_, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states) {
        auto self = (wayland::Connection*)self_;
        self->m_last_requested_width = width;
        self->m_last_requested_height = height;
        self->m_configure_requested = true;
        info("received: configure request: " + std::to_string(width) + "x" + std::to_string(height));
        // TODO: we should ack this, but how when we don't know the serial number?
    };
    m_toplevel_listener.close = [](void* self_, xdg_toplevel* toplevel) {
        auto self = (wayland::Connection*)self_;
        self->m_close_requested = true;
        info("received: close request");
    };
    m_toplevel_listener.configure_bounds = [](void* self_, xdg_toplevel* toplevel, int32_t width, int32_t height) {
        auto self = (wayland::Connection*)self_;
        self->m_recommended_max_width = width;
        self->m_recommended_max_height = height;
        info("received: recommended max dimensions: " + std::to_string(width) + "x" + std::to_string(height));
    };

    xdg_toplevel_add_listener(m_toplevel, &m_toplevel_listener, this);

    m_decoration_manager = bind_interface<zxdg_decoration_manager_v1>(&zxdg_decoration_manager_v1_interface, API_VERSION);
    if (!m_decoration_manager) {
        reset();
        throw std::runtime_error("Could not find Wayland decoration manager");
    }

    m_toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(m_decoration_manager, m_toplevel);
    if (!m_toplevel_decoration) {
        reset();
        throw std::runtime_error("Could not create a toplevel window decoration");
    }

    zxdg_toplevel_decoration_v1_set_mode(m_toplevel_decoration,
        ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void wayland::Connection::reset() {
    if (m_toplevel_decoration) {
        zxdg_toplevel_decoration_v1_destroy(m_toplevel_decoration);
        m_toplevel_decoration = nullptr;
    }
    if (m_decoration_manager) {
        zxdg_decoration_manager_v1_destroy(m_decoration_manager);
        m_decoration_manager = nullptr;
    }
    if (m_toplevel) {
        xdg_toplevel_destroy(m_toplevel);
        m_toplevel = nullptr;
    }
    if (m_xdg_surface) {
        xdg_surface_destroy(m_surface);
        m_xdg_surface = nullptr;
    }
    if (m_surface) {
        wl_surface_destroy(m_surface);
        m_surface = nullptr;
    }
    if (m_wm_base) {
        wl_base_destroy(m_wm_base);
        m_wm_base = nullptr;
    }
    if (m_seat) {
        wl_seat_destroy(m_seat);
        m_seat = nullptr;
    }
    if (m_shm) {
        wl_shm_destroy(m_shm);
        m_shm = nullptr;
    }
    if (m_compositor) {
        wl_compositor_destroy(m_compositor);
        m_compositor = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}

void wayland::Connection::pong(uint32_t serial_number) {
    xdg_wm_base_pong(m_wm_base, serial_number);
}

void wayland::Connection::ack_configure() {
    if (!m_configure_event_pending) {
        throw std::logic_error("ack_configure() but no configure event is pending");
    }
    xdg_surface_ack_configure(m_surface, m_last_configure_event_serial);
    m_configure_event_pending = false;
}

void wayland::Connection::roundtrip() {
    wl_display_roundtrip(m_display);
}

void wayland::Connection::set_window_geometry(int32_t x, int32_t y, int32_t width, int32_t height) {
    xdg_surface_set_window_geometry(m_xdg_surface, x, y, width, height);
}

void wayland::Connection::set_window_title(std::string title) {
    xdg_toplevel_set_title(m_toplevel, title.c_str());
}

int wayland::Connection::dispatch() {
    if (!m_display) {
        throw std::logic_error("dispatch() called with no display connection");
    }
    return wl_display_dispatch(m_display);
}