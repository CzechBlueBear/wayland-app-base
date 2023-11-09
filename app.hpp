#pragma once

#include <stdio.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "zxdg-decoration-client-protocol.h"
#include <array>
#include <map>
#include <string>
#include <linux/input-event-codes.h>
#include <memory>
#include <functional>

#include "box.hpp"
#include "draw.hpp"
#include "wayland_buffer.hpp"

namespace wl {

class Display {
private:
    wl_display* m_display = nullptr;
public:
    Display();
    ~Display();
    wl_display* get() { return m_display; }
    bool is_good() const { return !!m_display; }
    void roundtrip();
    int dispatch();
};

class Registry {
protected:
    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    std::map<std::string, uint32_t> m_interfaces;
public:
    Registry(Display& dpy);
    ~Registry();
    wl_registry* get() { return m_registry; }
    bool is_good() const { return !!m_registry; }

    template<class T>
    T* bind(const wl_interface* interface, uint32_t version) {
        auto cursor = m_interfaces.find(interface->name);
        if (cursor == m_interfaces.end()) { return nullptr; }
        return (T*)(wl_registry_bind(m_registry, cursor->second, interface, version));
    }
};

class Compositor {
protected:
    wl_compositor* m_compositor = nullptr;
public:
    const uint32_t API_VERSION = 4;
    Compositor(Registry& registry);
    ~Compositor();
    wl_compositor* get() { return m_compositor; }
    bool is_good() const { return !!m_compositor; }
};

class Shm {
protected:
    wl_shm* m_shm = nullptr;
public:
    const uint32_t API_VERSION = 1;
    Shm(Registry& registry);
    ~Shm();
    wl_shm* get() { return m_shm; }
    bool is_good() const { return !!m_shm; }
};

class Seat {
protected:
    wl_seat* m_seat = nullptr;
    struct wl_seat_listener m_listener = { 0 };
    std::string m_name = "";
    bool m_pointer_supported = false;
    bool m_keyboard_supported = false;
    bool m_touch_supported = false;
public:
    const uint32_t API_VERSION = 7;
    Seat(Registry& registry);
    ~Seat();
    wl_seat* get() { return m_seat; }
    bool is_good() const { return !!m_seat; }
    std::string get_name() const { return m_name; }
    bool is_pointer_supported() const { return m_pointer_supported; }
    bool is_keyboard_supported() const { return m_keyboard_supported; }
    bool is_touch_supported() const { return m_touch_supported; }
};

class Surface {
protected:
    wl_surface* m_surface = nullptr;
public:
    Surface(wl::Compositor& compositor);
    ~Surface();
    wl_surface* get() { return m_surface; }
    bool is_good() const { return !!m_surface; }
};

} // namespace wl

namespace xdg {
    namespace wm {

        /// Base interface of the window manager.
        class Base {
        protected:
            xdg_wm_base* m_base = nullptr;
            struct xdg_wm_base_listener m_listener = { 0 };
        public:
            const uint32_t API_VERSION = 1;
            Base(wl::Registry& registry);
            ~Base();
            xdg_wm_base* get() { return m_base; }
            bool is_good() const { return !!m_base; }
            void pong(uint32_t message_id);
        };

    } // namespace xdg::wm

    class Surface {
    protected:
        xdg_surface* m_surface = nullptr;
        struct xdg_surface_listener m_listener = { 0 };
    public:
        Surface(xdg::wm::Base& base, wl::Surface& low_surface);
        ~Surface();
        xdg_surface* get() { return m_surface; }
        bool is_good() const { return !!m_surface; }
    };

    class Toplevel {
    protected:
        xdg_toplevel* m_toplevel = nullptr;
        struct xdg_toplevel_listener m_listener = { 0 };
        bool m_close_requested = false;
    public:
        Toplevel(xdg::Surface& surface);
        ~Toplevel();
        xdg_toplevel* get() { return m_toplevel; }
        bool is_good() const { return !!m_toplevel; }
        bool is_close_requested() const { return m_close_requested; }
        void clear_close_request() { m_close_requested = false; }
        void set_title(std::string title);
    };

} // namespace xdg

class WaylandApp {
protected:
    static WaylandApp* the_app;

    // Wayland globals (existing on the server, we only bind to them)
    std::unique_ptr<wl::Display>    m_display;
    std::unique_ptr<wl::Registry>   m_registry;
    std::unique_ptr<wl::Compositor> m_compositor;
    std::unique_ptr<wl::Shm>        m_shm;
    std::unique_ptr<xdg::wm::Base>  m_base;
    std::unique_ptr<wl::Seat>       m_seat;

    // Wayland objects (owned by us)
    std::unique_ptr<wl::Surface>    m_surface;
    std::unique_ptr<xdg::Surface>   m_xdg_surface;
    std::unique_ptr<xdg::Toplevel>  m_toplevel;

    static constexpr int BUFFER_COUNT = 4;
    WaylandBuffer m_buffers[BUFFER_COUNT];

    int m_window_width = DEFAULT_WINDOW_WIDTH;
    int m_window_height = DEFAULT_WINDOW_HEIGHT;
    bool m_close_requested = false;

    bool m_redraw_needed = false;

public:

    static const int DEFAULT_WINDOW_WIDTH = 1280;
    static const int DEFAULT_WINDOW_HEIGHT = 1024;

    WaylandApp();
    virtual ~WaylandApp();
    bool is_good() const { return !!m_toplevel; }

    static WaylandApp &the();

    void enter_event_loop();
    void render_frame();
    bool is_close_requested() const { return m_close_requested; }

    // 1st level event handlers
    // these are called by the wayland-client library
    static void on_buffer_release(void* data, wl_buffer* buffer);
    static void on_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
    static void on_xdg_toplevel_configure(void* data, struct xdg_toplevel* xdg_toplevel, int32_t width,
                    int32_t height, struct wl_array *states);
    static void on_xdg_toplevel_close(void* data, struct xdg_toplevel *xdg_toplevel);
    static void on_seat_handle_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities);
    static void on_seat_name(void* data, struct wl_seat* seat, char const* name);
    static void on_pointer_button(void* data, struct wl_pointer *pointer,
                    uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
    static void on_pointer_enter(void* data, struct wl_pointer* pointer,
                    uint32_t serial, wl_surface*, wl_fixed_t x, wl_fixed_t y);
    static void on_pointer_leave(void* data, struct wl_pointer* pointer,
                    uint32_t serial, wl_surface*);
    static void on_pointer_motion(void *data, struct wl_pointer *wl_pointer,
               uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
    static void on_pointer_frame(void* data, struct wl_pointer* wl_pointer);

    // 2nd level event handlers
    virtual void draw(DrawingContext ctx);

    //void register_global(char const* interface, uint32_t name);
};
