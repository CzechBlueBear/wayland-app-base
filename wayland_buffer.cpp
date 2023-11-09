#include "wayland_buffer.hpp"
#include <cassert>
#include <cstdio>

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

    // use a single pool per buffer; from the docs it seems there is
    // no benefit in having multiple buffers in a single pool
    wl_shm_pool *pool = wl_shm_create_pool(shm, m_memory.get_fd(), size);
    if (!pool) {
        perror("WaylandBuffer::setup(): wl_shm_create_pool()");
        return false;
    }

    // allocate the given size of the buffer from the pool
    m_buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    if (!m_buffer) {
        perror("WaylandBuffer::setup(): wl_shm_pool_create_buffer()");
        return false;
    }

    // the pool is not useful anymore (strange but according to Wayland docs)
    wl_shm_pool_destroy(pool);

    m_width = width;
    m_height = height;

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

    // static wl_buffer_listener listener {
    //     .release = [](void* self, wl_buffer*) {
    //         assert(((WaylandBuffer*)self)->m_busy);
    //         ((WaylandBuffer*)self)->m_busy = false;
    //     }
    // };
    int ret = wl_buffer_add_listener(m_buffer, &m_listener_info, this);
    if (ret != 0) {
        perror("WaylandBuffer::setup(): wl_buffer_add_listener()");
        // TODO: what to do here?
    }

    m_busy = true;
    wl_surface_attach(surface, m_buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, UINT32_MAX, UINT32_MAX);
    wl_surface_commit(surface);
}

void WaylandBuffer::reset() {
    assert(!m_busy);
    if (!m_valid) {
        return;
    }

    wl_buffer_destroy(m_buffer);

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
