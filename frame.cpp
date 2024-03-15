#include "frame.hpp"
#include "app.hpp"

// request GNU-specific definitions (memfd_create(), MFD_CLOEXEC, MFD_ALLOW_SEALING)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/mman.h>
#include <cassert>
#include <stdexcept>

wayland::Frame::Frame(wayland::Display& display, int32_t width, int32_t height) {
    assert(width >= 0 && height >= 0);

    int32_t size = width*height*4;  // 4 bytes per pixel (XRGB or ARGB)
    int32_t stride = width*4;       // each row is width*4 bytes wide

    // an anonymous in-memory file to share with the Wayland server
    int fd = memfd_create("frame", MFD_CLOEXEC|MFD_ALLOW_SEALING);
    if (!fd) {
        throw std::runtime_error("wayland::Frame: memfd_create() failed: " + errno_to_string());
    }
    for(;;) {
        int ret = ftruncate(fd, size);
        if (ret == 0) { break; }
        if (errno != EINTR) {
            auto orig_errno = errno;
            close(fd);
            throw std::runtime_error("wayland::Frame: ftruncate() failed: " + errno_to_string(orig_errno));
        }
    }
    void* memory = mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (memory == MAP_FAILED) {
        auto orig_errno = errno;
        close(fd);
        throw std::runtime_error("wayland::Frame: mmap() failed: " + errno_to_string(orig_errno));
    }

    // temporary pool to allocate the frame from (deleted at the end of this call)
    std::unique_ptr<wl_shm_pool, wl_shm_pool_deleter> pool(
        wl_shm_create_pool(display.get_shm().get(), fd, size));
    if (!pool) {
        auto orig_errno = errno;
        munmap(memory, size);
        close(fd);
        throw std::runtime_error("wayland::Frame: wl_shm_create_pool() failed: " + errno_to_string(orig_errno));
    }

    // allocate the given size of the buffer from the pool
    std::unique_ptr<wl_buffer, wl_buffer_deleter> buffer {
        wl_shm_pool_create_buffer(pool.get(), 0, width, height, stride, WL_SHM_FORMAT_XRGB8888)
    };
    if (!buffer) {
        auto orig_errno = errno;
        munmap(memory, size);
        close(fd);
        throw std::runtime_error("wayland::Frame: wl_shm_pool_create_buffer() failed: " + errno_to_string(orig_errno));
    }

    // close filedescriptor to conserve them; the mapping stays
    close(fd);

    // install a listener to inform us when the Wayland server releases the buffer
    m_listener.release = [](void* self_, wl_buffer* buffer) {
        auto self = (wayland::Frame*) self_;
        self->m_buffer_busy = false;
    };
    wl_buffer_add_listener(buffer.get(), &m_listener, this);

    m_memory = memory;
    m_size = size;
    m_width = width;
    m_height = height;
    m_buffer = std::move(buffer);
}

wayland::Frame::~Frame() {
    // assert(!m_buffer_busy);  // this is allowed when destroying the window
    munmap(m_memory, m_size);
}

void wayland::Frame::attach(wayland::Window& window) {
    assert(!m_buffer_busy);
    m_buffer_busy = true;
    wl_surface_attach(window.get_surface().get(), m_buffer.get(), 0, 0);
}
