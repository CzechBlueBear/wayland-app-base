#include <stdio.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <array>
#include <map>
#include <string>

class WaylandApp {
protected:
    static WaylandApp* the_app;

    // Wayland globals
    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    wl_compositor* m_compositor = nullptr;
    wl_shm* m_shm = nullptr;
    xdg_wm_base* m_xdg_wm_base = nullptr;

    // Wayland objects
    wl_surface* m_surface = nullptr;
    xdg_surface* m_xdg_surface = nullptr;
    xdg_toplevel* m_xdg_toplevel = nullptr;

    std::map<std::string, uint32_t> m_globals;

    int m_window_width = DEFAULT_WINDOW_WIDTH;
    int m_window_height = DEFAULT_WINDOW_HEIGHT;

public:

    static const int DEFAULT_WINDOW_WIDTH = 1280;
    static const int DEFAULT_WINDOW_HEIGHT = 1024;

    WaylandApp();
    virtual ~WaylandApp();

    WaylandApp &the();

    bool connect();
    void disconnect();
    void handle_events();
    void render_frame();

    // event handlers
    static void on_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial);
    static void on_registry_handle_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void on_registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name);
    static void on_buffer_release(void* data, wl_buffer* buffer);
    static void on_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);

protected:
    void register_global(char const* interface, uint32_t name);
    void bind_to_compositor(uint32_t name);
    void bind_to_shm(uint32_t name);
    void bind_to_xdg_wm_base(uint32_t name);
    wl_buffer* allocate_buffer(uint32_t width, uint32_t height);
    void present_buffer(wl_buffer* buffer);

    void complain(char const* message);
};
