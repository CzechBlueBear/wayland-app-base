#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <wayland-client.h>
#include "debug.hpp"
#include "xdg-shell-client-protocol.h"
#include "zxdg-decoration-client-protocol.h"
#include <linux/input-event-codes.h>
#include <unistd.h>

#include "frame.hpp"
#include "draw.hpp"

#if USE_EGL
#include <wayland-egl.h>
#endif

struct wl_shm_pool_deleter {
    void operator()(wl_shm_pool* p) { wl_shm_pool_destroy(p); }
};

namespace wl {

/**
 * Base class for objects that wrap Wayland objects.
 * These cannot be copied nor trivially constructed.
 */
class WaylandObject {
public:
    WaylandObject() {}
    WaylandObject(WaylandObject const&) = delete;
    WaylandObject& operator=(WaylandObject const&) = delete;
    virtual ~WaylandObject() {}
};

/// Represents the connection to the Wayland display (encapsulates wl_display).
class Connection : public WaylandObject {
private:
    wl_display* m_display = nullptr;
    wl_display_listener m_listener = { 0 };
public:
    Connection();
    ~Connection();
    operator wl_display*() { return m_display; }
    wl_display* get() { return m_display; }
    void roundtrip();

    /// Handles all currently pending incoming events on the connection,
    /// calling appropriate callbacks and updating object states.
    int dispatch_events();

    /// Flushes outgoing events.
    void flush_events();

    /// Returns the file descriptor of the connection, useful for waiting
    /// for events with poll() or similar call.
    int get_fd();
};

/**
 * Registry of API interfaces that are supported by the Wayland server.
 */
class Registry : public WaylandObject {
protected:
    wl_registry* m_registry = nullptr;
    std::map<std::string, uint32_t> m_interfaces;
public:
    Registry(Connection& conn);
    ~Registry();
    wl_registry* operator*() { return m_registry; }

    /**
     * Checks whether an interface of that name is supported.
     * The server announces the supported interfaces in the first roundtrip
     * after we connect.
     */
    bool has_interface(std::string interface_name) {
        auto cursor = m_interfaces.find(interface_name);
        return (cursor != m_interfaces.end());
    }

    /// Requests the given Wayland interface with specified version,
    /// and announces the use of it to the Wayland server.
    /// Returns a pointer to a structure representing the interface
    /// (the caller must know what to cast the pointer to).
    /// The resulting structure must be later destroyed via appropriate method
    /// (usually some destroy() version specific to the structure).
    /// On failure, null is returned.
    void* bind_interface(const wl_interface* interface, uint32_t version);
};

class Compositor : public WaylandObject {
protected:
    wl_compositor* m_compositor = nullptr;
public:
    const uint32_t API_VERSION = 4;
    Compositor(Registry& registry);
    ~Compositor();
    wl_compositor* get() { return m_compositor; }
};

class Shm : public WaylandObject {
protected:
    wl_shm* m_shm = nullptr;
public:
    const uint32_t API_VERSION = 1;
    Shm(Registry& registry);
    ~Shm();
    wl_shm* get() { return m_shm; }
};

class Seat : public WaylandObject {
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
    std::string get_name() const { return m_name; }
    bool is_pointer_supported() const { return m_pointer_supported; }
    bool is_keyboard_supported() const { return m_keyboard_supported; }
    bool is_touch_supported() const { return m_touch_supported; }
};

class Region;

class Surface : public WaylandObject {
protected:
    wl_surface* m_surface = nullptr;
public:
    Surface(wl::Compositor& compositor);
    ~Surface();
    wl_surface* get() { return m_surface; }
    void commit();
    void damage(int32_t x, int32_t y, int32_t width, int32_t height);
    void set_opaque_region(Region& region);
    void remove_opaque_region();
};

class Output : public WaylandObject {
protected:
    wl_output* m_output = nullptr;
public:
    Output(wl::Registry& registry);
    ~Output();
    wl_output* get() { return m_output; }
};

/// Represents an area composed of one or more rectangles;
/// used, among others, for marking opaque and input-sensitive areas.
/// @see https://wayland-book.com/surfaces-in-depth/surface-regions.html
class Region : public WaylandObject {
protected:
    wl_region* m_region = nullptr;
public:
    Region(wl::Compositor& compositor);
    ~Region();
    wl_region* get() { return m_region; }
    void add(int32_t x, int32_t y, int32_t width, int32_t height);
    void subtract(int32_t x, int32_t y, int32_t width, int32_t height);
};

#if USE_EGL

class EGLWindow : public WaylandObject {
protected:
    wl_egl_window* m_window = nullptr;
public:
    EGLWindow(wl::Surface& surface, int width, int height);
    ~EGLWindow();
    wl_egl_window* get() { return m_window; }
};

#endif

} // namespace wl

namespace xdg {
    namespace wm {

        /// Base interface of the window manager.
        class Base : public wl::WaylandObject {
        protected:
            struct xdg_wm_base* m_base = nullptr;
            struct xdg_wm_base_listener m_listener = { 0 };
        public:
            const uint32_t API_VERSION = 1;
            Base(wl::Registry& registry);
            ~Base();
            xdg_wm_base* get() { return m_base; }
            void pong(uint32_t message_id);
        };

    } // namespace xdg::wm

    class Surface : public wl::WaylandObject {
    protected:
        struct xdg_surface* m_surface = nullptr;
        struct xdg_surface_listener m_listener = { 0 };
        uint32_t m_last_configure_event_serial = 0;
        bool m_configure_event_pending = false;
    public:
        Surface(xdg::wm::Base& base, wl::Surface& low_surface);
        ~Surface();
        xdg_surface* get() { return m_surface; }
        bool is_configure_event_pending() const { return m_configure_event_pending; }
        void ack_configure();
        void set_window_geometry(int32_t x, int32_t y, int32_t width, int32_t height);
    };

    // The Wayland equivalent of a window encapsulating a surface.
    class Toplevel : public wl::WaylandObject {
    protected:
        struct xdg_toplevel* m_toplevel = nullptr;
        struct xdg_toplevel_listener m_listener = { 0 };
        bool m_close_requested = false;
        bool m_configure_requested = false;
        int m_last_requested_width = 0;
        int m_last_requested_height = 0;
        int32_t m_recommended_max_width = 0;
        int32_t m_recommended_max_height = 0;
   public:
        Toplevel(xdg::Surface& surface);
        ~Toplevel();
        xdg_toplevel* get() { return m_toplevel; }
        bool is_close_requested() const { return m_close_requested; }
        void clear_close_request() { m_close_requested = false; }
        bool is_configure_requested() const { return m_configure_requested; }
        void clear_configure_request() { m_configure_requested = false; }
        void set_title(std::string title);
        int32_t get_last_requested_width() const { return m_last_requested_width; }
        int32_t get_last_requested_height() const { return m_last_requested_height; }
        int32_t get_recommended_max_width() const { return m_recommended_max_width; }
        int32_t get_recommended_max_height() const { return m_recommended_max_height; }
    };

    class DecorationManager : public wl::WaylandObject {
    protected:
        struct zxdg_decoration_manager_v1* m_manager = nullptr;
        const int API_VERSION = 1;
    public:
        DecorationManager(wl::Registry& registry);
        ~DecorationManager();
        zxdg_decoration_manager_v1* get() { return m_manager; }
        static bool is_supported(wl::Registry& registry);
    };

    class ToplevelDecoration : public wl::WaylandObject {
    protected:
        struct zxdg_toplevel_decoration_v1* m_decoration = nullptr;
    public:
        ToplevelDecoration(xdg::DecorationManager& manager, xdg::Toplevel& toplevel);
        ~ToplevelDecoration();
        zxdg_toplevel_decoration_v1* get() { return m_decoration; }
        void set_server_side_mode();
    };

} // namespace xdg

namespace wayland {

class Display {
protected:
    std::unique_ptr<wl::Connection>     m_connection;
    std::unique_ptr<wl::Registry>       m_registry;
    std::unique_ptr<wl::Compositor>     m_compositor;
    std::unique_ptr<wl::Shm>            m_shm;
    std::unique_ptr<wl::Seat>           m_seat;
    std::unique_ptr<wl::Output>         m_output;
    std::unique_ptr<xdg::wm::Base>      m_wm_base;
    std::unique_ptr<xdg::DecorationManager> m_decoration_manager;
public:
    Display();
    wl::Connection& get_connection() { return *m_connection; }
    wl::Registry& get_registry() { return *m_registry; }
    wl::Compositor& get_compositor() { return *m_compositor; }
    wl::Shm& get_shm() { return *m_shm; }
    wl::Seat& get_seat() { return *m_seat; }
    xdg::wm::Base& get_wm_base() { return *m_wm_base; }
    bool has_decoration_manager() { return !!m_decoration_manager; }
    xdg::DecorationManager& get_decoration_manager();
};

class Window {
protected:
    std::unique_ptr<wl::Surface>    m_surface;
    std::unique_ptr<xdg::Surface>   m_xdg_surface;
    std::unique_ptr<xdg::Toplevel>  m_toplevel;
    std::unique_ptr<xdg::ToplevelDecoration> m_decoration;
#if USE_EGL
    std::unique_ptr<wl::EGLWindow>  m_egl_window;
#endif
public:
    Window(wayland::Display& display);
    ~Window() {}
    wl::Surface& get_surface() { return *m_surface; }
    xdg::Surface& get_xdg_surface() { return *m_xdg_surface; }
    xdg::Toplevel& get_toplevel() { return *m_toplevel; }
};

} // namespace wayland

class WaylandApp {
protected:
    static WaylandApp* the_app;

    std::unique_ptr<wayland::Display> m_display;
    std::unique_ptr<wayland::Window> m_window;

    int m_window_width = DEFAULT_WINDOW_WIDTH;
    int m_window_height = DEFAULT_WINDOW_HEIGHT;
    bool m_close_requested = false;

    bool m_redraw_needed = false;

    std::list<wayland::Frame*> m_frames;
    wayland::Frame* get_new_frame(int32_t width, int32_t height);
    void purge_badly_sized_frames(int32_t width, int32_t height);

public:

    static const int DEFAULT_WINDOW_WIDTH = 1280;
    static const int DEFAULT_WINDOW_HEIGHT = 1024;

    WaylandApp();
    virtual ~WaylandApp();

    static WaylandApp &the();

    void reconfigure_buffer(int index, int32_t width, int32_t height);
    int find_unused_buffer();

    void enter_event_loop();
    void render_frame(wayland::Frame& frame);
    bool is_close_requested() const { return m_close_requested; }

    // 2nd level event handlers
    virtual void draw(DrawingContext ctx);
};
