#pragma once
#include <wayland-client.h>
#include <cassert>
#include "shm_util.hpp"

class WaylandBuffer {
private:
    bool m_valid = false;
    bool m_busy = false;
    wl_buffer* m_buffer = nullptr;
    int m_width = 0;
    int m_height = 0;
    AnonSharedMemory m_memory;
    wl_buffer_listener m_listener_info;
    static void on_release(void* self, wl_buffer* buffer);
public:
    ~WaylandBuffer();
    bool setup(wl_shm* shm, int width, int height);
    void reset();
    void present(wl_surface* surface);
    bool is_valid() const { return m_valid; }
    bool is_busy() const { assert(m_valid); return m_busy; }
    int get_width() const { assert(m_valid); return m_width; }
    int get_height() const { assert(m_valid); return m_height; }
    bool map();
    void unmap();
    void* get_pixels() { assert(m_valid); assert(!m_busy); return m_memory.get_memory(); }
};
