#include <stdio.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <array>
#include <map>
#include <string>

struct BufferInfo {
    bool attached = false;
};

class WaylandApp {
protected:
    static WaylandApp* the_app;

    // Wayland globals
    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    std::map<std::string, uint32_t> m_globals;
    wl_compositor* m_compositor = nullptr;
    wl_shm* m_shm = nullptr;
    xdg_wm_base* m_xdg_wm_base = nullptr;

    // Wayland objects
    wl_shm_pool *m_shm_pool = nullptr;
    wl_surface* m_surface = nullptr;
    xdg_surface* m_xdg_surface = nullptr;
    xdg_toplevel* m_xdg_toplevel = nullptr;

/*
    std::array<wl_buffer*, 2> m_buffers { nullptr, nullptr };
    std::array<BufferInfo, 2> m_buffer_infos;
    size_t m_shm_pool_size = 0;
    size_t m_pool_stride = 0;
    uint8_t* m_pool_data = nullptr;
*/

public:

    static const int DEFAULT_WINDOW_WIDTH = 1280;
    static const int DEFAULT_WINDOW_HEIGHT = 1024;

    WaylandApp();
    virtual ~WaylandApp();

    WaylandApp &the();

    bool connect();
    void disconnect();
    void handle_events();
    wl_buffer* allocate_buffer(uint32_t width, uint32_t height);
    void present_buffer(wl_buffer* buffer);

    // event handlers
    static void on_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial);
    static void on_registry_handle_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void on_registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name);
    static void on_buffer_release(void* data, wl_buffer* buffer);

protected:
    //bool create_buffer_pool(uint32_t width, uint32_t height);
    //bool create_buffer(int index, uint32_t width, uint32_t height);
    void register_global(char const* interface, uint32_t name);
    void bind_to_compositor(uint32_t name);
    void bind_to_shm(uint32_t name);
    void bind_to_xdg_wm_base(uint32_t name);
};
