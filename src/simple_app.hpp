#pragma once

#include <cstdint>
#include <list>
#include <wayland-client.h>
#include <wayland-egl.h>

namespace wayland {

class Connection {
private:
    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    wl_compositor* m_compositor = nullptr;
    wl_shm* m_shm = nullptr;
    wl_seat* m_seat = nullptr;
    wl_surface* m_surface = nullptr;
    wl_egl_window* m_window = nullptr;
    xdg_wm_base* m_wm_base = nullptr;
    xdg_surface* m_xdg_surface = nullptr;
    xdg_toplevel* m_toplevel = nullptr;
    zxdg_decoration_manager_v1* m_decoration_manager = nullptr;
    zxdg_toplevel_decoration_v1* m_toplevel_decoration = nullptr;

    const uint32_t COMPOSITOR_API_VERSION = 4;
    const uint32_t SHM_API_VERSION = 1;
    const uint32_t SEAT_API_VERSION = 7;
    const uint32_t WM_BASE_API_VERSION = 1;
    const uint32_t DECORATION_MANAGER_API_VERSION = 1;

    struct wl_display_listener m_display_listener = { 0 };
    struct wl_registry_listener m_registry_listener = { 0 };
    struct wl_seat_listener m_seat_listener = { 0 };
    struct xdg_wm_base_listener m_wm_base_listener = { 0 };
    struct xdg_surface_listener m_xdg_surface_listener = { 0 };
    struct xdg_toplevel_listener m_toplevel_listener = { 0 };

    std::map<std::string, uint32_t> m_interfaces;
    std::string m_seat_name = "";
    uint32_t m_last_configure_event_serial = 0;
    bool m_configure_event_pending = false;
    bool m_close_requested = false;
    bool m_configure_requested = false;
    int m_last_requested_width = 0;
    int m_last_requested_height = 0;
    int32_t m_recommended_max_width = 0;
    int32_t m_recommended_max_height = 0;
    bool m_pointer_supported = false;
    bool m_keyboard_supported = false;
    bool m_touch_supported = false;
    int32_t m_window_width = 640;
    int32_t m_window_height = 480;
protected:
    /**
     * Checks whether an interface of that name is supported.
     * The server announces the supported interfaces in the first roundtrip
     * after we connect.
     */
    template<class T>
    bool has_interface(std::string interface_name) {
        auto cursor = m_interfaces.find(interface_name);
        return (cursor != m_interfaces.end());
    }

    /**
     * Requests the given Wayland interface with specified version,
     * and announces the use of it to the Wayland server.
     * Returns the structure representing the interface.
     * Returns null if this fails (the server does not support the interface).
     */
    template<class T>
    T* bind_interface(const wl_interface* interface, uint32_t version) {
        auto cursor = m_interfaces.find(interface->name);
        if (cursor == m_interfaces.end()) { return nullptr; }
        auto result = (T*)(wl_registry_bind(m_registry, cursor->second, interface, version));
        if (result) {
            info(std::string("bound to interface: ") + interface->name);
        }
        else {
            info(std::string("could not bind to interface: ") + interface->name);
        }
        return result;
    }
    /**
     * Sends a reply to the window-manager ping request that determines
     * whether an application is still responsive.
     * Called automatically from the wm_base_listener callback.
     */
    void pong(uint32_t serial_number);
    void ack_configure();
    void roundtrip();
public:
    Connection();
    virtual ~Connection() { reset(); }
    void connect();
    /**
     * Abruptly but safely terminates the connection (if still pending)
     * and resets the Connection object to the default state.
     */
    void reset();
    void set_window_geometry(int32_t x, int32_t y, int32_t width, int32_t height);
    void set_window_title(std::string title);
    int dispatch();
};

}