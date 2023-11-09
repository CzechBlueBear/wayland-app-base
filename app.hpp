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
    bool is_good() { return !!m_display; }
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
    bool is_good() { return !!m_registry; }
    uint32_t get_interface_name(std::string interface) { return m_interfaces[interface]; }
    void* bind(uint32_t name, const wl_interface* interface, uint32_t version);

    template<class T>
    T* bind(const wl_interface* interface, uint32_t version) {
        auto cursor = m_interfaces.find(interface->name);
        if (cursor == m_interfaces.end()) { return nullptr; }
        return (T*)(wl_registry_bind(m_registry, cursor->second, interface, version));
    }
};

} // namespace wl

class WaylandApp {
protected:
    static WaylandApp* the_app;

    // Wayland globals
    std::unique_ptr<wl::Display> m_display;
    std::unique_ptr<wl::Registry> m_registry;
    struct wl_compositor_deleter { void operator()(wl_compositor* c) { wl_compositor_destroy(c); }};
    std::unique_ptr<wl_compositor, wl_compositor_deleter> m_compositor;
    struct wl_shm_deleter { void operator()(wl_shm* shm) { wl_shm_destroy(shm); }};
    std::unique_ptr<wl_shm, wl_shm_deleter> m_shm;
    struct xdg_wm_base_deleter { void operator()(xdg_wm_base* base) { xdg_wm_base_destroy(base); }};
    std::unique_ptr<xdg_wm_base, xdg_wm_base_deleter> m_xdg_wm_base;
    struct wl_seat_deleter { void operator()(wl_seat* s) { wl_seat_destroy(s); }};
    std::unique_ptr<wl_seat, wl_seat_deleter> m_seat;

    // Wayland objects
    struct wl_surface_deleter { void operator()(wl_surface* s) { wl_surface_destroy(s); }};
    std::unique_ptr<wl_surface, wl_surface_deleter> m_surface;
    struct xdg_surface_deleter { void operator()(xdg_surface* s) { xdg_surface_destroy(s); }};
    std::unique_ptr<xdg_surface, xdg_surface_deleter> m_xdg_surface;
    struct xdg_toplevel_deleter { void operator()(xdg_toplevel* tl) { xdg_toplevel_destroy(tl); }};
    std::unique_ptr<xdg_toplevel, xdg_toplevel_deleter> m_xdg_toplevel;

    static wl_registry_listener wl_registry_listener_info;
    static xdg_wm_base_listener xdg_wm_base_listener_info;

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

    static WaylandApp &the();

    bool connect();
    void disconnect();
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
