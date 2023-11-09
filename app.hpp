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

class WaylandApp {
protected:
    static WaylandApp* the_app;

    // Wayland globals
    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    wl_compositor* m_compositor = nullptr;
    wl_shm* m_shm = nullptr;
    xdg_wm_base* m_xdg_wm_base = nullptr;
    wl_seat* m_seat = nullptr;

    // Wayland objects
    wl_surface* m_surface = nullptr;
    xdg_surface* m_xdg_surface = nullptr;
    xdg_toplevel* m_xdg_toplevel = nullptr;

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

    WaylandApp &the();

    bool connect();
    void disconnect();
    void enter_event_loop();
    void render_frame();
    bool is_close_requested() const { return m_close_requested; }

    // 1st level event handlers
    // these are called by the wayland-client library
    static void on_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial);
    static void on_registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void on_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name);
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

protected:
    void register_global(char const* interface, uint32_t name);
};
