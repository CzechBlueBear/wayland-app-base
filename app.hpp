#include <stdio.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <array>
#include <map>
#include <string>

class WaylandApp {
protected:
    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    std::map<std::string, uint32_t> m_globals;
    wl_compositor* m_compositor = nullptr;
    wl_shm* m_shm = nullptr;
    wl_shm_pool *m_shm_pool = nullptr;
    wl_surface* m_surface = nullptr;
    std::array<wl_buffer*, 2> m_buffers { nullptr, nullptr };
    size_t m_shm_pool_size = 0;
    size_t m_pool_stride = 0;
    uint8_t* m_pool_data = nullptr;
    xdg_wm_base* m_xdg_wm_base = nullptr;
    xdg_surface* m_xdg_surface = nullptr;
    xdg_toplevel* m_xdg_toplevel = nullptr;

public:
    WaylandApp();

    bool connect();
    void disconnect();
    void handle_events();
    void bind_to_compositor(uint32_t name);
    void bind_to_shm(uint32_t name);
    void bind_to_xdg_wm_base(uint32_t name);
    bool create_buffer_pool(uint32_t width, uint32_t height);
    void attach_and_redraw_buffer(int buffer_index);
    void register_global(char const* interface, uint32_t name);

protected:
    bool create_buffer(int index, uint32_t width, uint32_t height);

    static void callback_registry_handle_global(void *data, wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version);
    static void callback_xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, uint32_t serial);
};
