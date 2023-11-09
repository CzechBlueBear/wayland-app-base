#include "wayland_buffer.hpp"
#include "debug.hpp"
#include <cassert>
#include <cstdio>
#include <memory>

WaylandBuffer::~WaylandBuffer() {
    reset();
}

bool WaylandBuffer::setup(wl_shm* shm, int width, int height) {
    assert(!m_valid);

    // 4 bytes per pixel (XRGB), so each line is 4*width
    int stride = 4*width;
    int size = stride*height;
    if (!m_memory.open(size)) {
        return false;
    }

    // use a single temporary pool for the whole buffer
    struct wl_shm_pool_deleter { void operator()(wl_shm_pool* p) { wl_shm_pool_destroy(p); }};
    std::unique_ptr<wl_shm_pool, wl_shm_pool_deleter> pool(
        wl_shm_create_pool(shm, m_memory.get_fd(), size));
    if (!pool) {
        complain("wl_shm_create_pool() failed");
        return false;
    }

    // allocate the given size of the buffer from the pool
    m_buffer.reset(wl_shm_pool_create_buffer(pool.get(), 0, width, height, stride, WL_SHM_FORMAT_XRGB8888));
    if (!m_buffer) {
        complain("wl_shm_pool_create_buffer() failed");
        return false;
    }

    m_width = width;
    m_height = height;

    wl_buffer_add_listener(m_buffer.get(), &m_listener_info, this);

    m_busy = false;
    m_valid = true;
    return true;
}

void WaylandBuffer::on_release(void* self, wl_buffer* buffer) {
    assert(((WaylandBuffer*)self)->m_busy);
    ((WaylandBuffer*)self)->m_busy = false;
}

void WaylandBuffer::present(wl_surface* surface) {
    assert(surface);
    assert(m_valid);
    assert(!m_busy);

    m_listener_info.release = on_release;

    m_busy = true;
    wl_surface_attach(surface, m_buffer.get(), 0, 0);
    wl_surface_damage(surface, 0, 0, UINT32_MAX, UINT32_MAX);
    wl_surface_commit(surface);
}

void WaylandBuffer::reset() {
    assert(!m_busy);
    if (!m_valid) {
        return;
    }

    m_buffer.reset();

    m_memory.unmap();
    m_memory.close();

    m_buffer = nullptr;
    m_width = 0;
    m_height = 0;
    m_busy = false;
    m_valid = false;
}

bool WaylandBuffer::map() {
    return m_memory.map();
}

void WaylandBuffer::unmap() {
    m_memory.unmap();
}
