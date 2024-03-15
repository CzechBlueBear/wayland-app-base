#pragma once

#include <wayland-client.h>
#include <memory>

struct wl_buffer_deleter {
    void operator()(wl_buffer* buf) { wl_buffer_destroy(buf); }
};

namespace wayland {

class Display;
class Window;

/**
 * A renderable frame placed in a memory-mapped buffer shared with the Wayland server.
 * It wraps both the m_buffer structure that describes the memory in Wayland parlance,
 * and the mmap-ed block of memory.
 * Lifecycle:
 *    1. create a new Frame using the constructor
 *    2. draw into it by directly accessing its memory (get_memory())
 *    3. make it eligible for drawing by calling attach() to a window
 *    4. do not touch it until is_busy() returns false
 * Can throw std::runtime_error if the allocations fail.
 * Normally, this is all done by the WaylandApp class internally.
 */
class Frame {
protected:
    void*   m_memory = nullptr;
    int32_t m_size = 0;
    int32_t m_width = 0;
    int32_t m_height = 0;
    std::unique_ptr<wl_buffer, wl_buffer_deleter> m_buffer;
    wl_buffer_listener m_listener = { 0 };
    bool    m_buffer_busy = false;
public:
    Frame(wayland::Display& display, int32_t width, int32_t height);
    ~Frame();
    void attach(wayland::Window& window);
    void* get_memory() { return m_memory; }
    int32_t get_width() const { return m_width; }
    int32_t get_height() const { return m_height; }
    bool is_busy() const { return m_buffer_busy; }
};

}
