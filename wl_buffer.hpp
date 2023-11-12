#pragma once
#include <wayland-client-protocol.h>

namespace wl {

class Shm;

class ShmPool {
protected:
    wl_shm_pool* m_pool = nullptr;
    int m_fd = -1;
    int32_t m_size = 0;
public:
    ShmPool(Shm& shm, int32_t size);
    ~ShmPool();
    wl_shm_pool* get() { return m_pool; }
    int get_fd() { return m_fd; }
};

class Buffer {
protected:
    wl_buffer* m_buffer = nullptr;
    wl_buffer_listener m_listener = { 0 };
    bool m_busy = false;
    int32_t m_width = 0;
    int32_t m_height = 0;
    int32_t m_stride = 0;
    uint32_t m_format = 0;
    void* m_mapped_memory = nullptr;
public:
    Buffer(ShmPool& pool, int32_t offset, int32_t width, int32_t height);
    ~Buffer();
    wl_buffer* get() { return m_buffer; }
    bool is_good() const { return !!m_buffer; }
    bool is_busy() const { return m_busy; }
    int32_t get_width() const { return m_width; }
    int32_t get_height() const { return m_height; }
    int32_t get_stride() const { return m_stride; }
    uint32_t get_format() const { return m_format; }
    void map();
    void unmap();
};

} // namespace wl
