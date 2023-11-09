#include "debug.hpp"
#include "shm_util.hpp"

#include <cstdio>
#include <cassert>
#include <ctime>
#include <cerrno>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

AnonSharedMemory::~AnonSharedMemory() {
    unmap();
    close();
}

bool AnonSharedMemory::open(size_t size) {
    if (m_fd >= 0) {
        complain("AnonSharedMemory::open(): another fd still open");
        return false;
    }
    if (m_memory) {
        complain("AnonSharedMemory::open(): memory still mapped");
        return false;
    }

    auto fd = memfd_create("anon", 0);
    if (fd < 0) {
        complain("AnonSharedMemory::open(): memfd_create() failed");
        return false;
    }

    int ret;
    do {
        ret = ftruncate(fd, size);
    }
    while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        complain("AnonSharedMemory::open(): ftruncate() failed");
        ::close(fd);
        return false;
    }

    m_fd = fd;
    m_size = size;
    return true;
}

void AnonSharedMemory::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool AnonSharedMemory::map() {
    if (m_fd < 0) {
        complain("AnonSharedMemory::map(): fd is not open");
        return false;
    }
    if (m_memory) {
        complain("AnonSharedMemory::map(): already mapped");
        return false;
    }

    auto memory = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (memory == MAP_FAILED) {
        complain("AnonSharedMemory::map(): mmap() failed");
        return false;
    }
    m_memory = memory;

    return true;
}

void AnonSharedMemory::unmap() {
    if (m_memory) {
        munmap(m_memory, m_size);
        m_memory = nullptr;
    }
}
