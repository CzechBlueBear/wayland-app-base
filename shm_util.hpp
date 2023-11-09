#pragma once
#include <unistd.h>
#include <cassert>

/// Represents an anonymous block of memory that can be shared between processes.
class AnonSharedMemory {
private:
    int m_fd = -1;
    void* m_memory = nullptr;
    size_t m_size = 0;
public:
    ~AnonSharedMemory();

    /// Opens an unnamed shareable in-memory file and sets its size.
    /// Returns true on success, false on failure.
    bool open(size_t size);

    /// Closes the underlying file, freeing its file descriptor for reuse.
    /// If the file is mapped in memory, the mapping is not affected.
    /// Has no effect if the underlying file descriptor is not open.
    void close();

    /// Maps the underlying file to memory space of the calling process.
    /// Returns true on success, false on failure (usually if out of space).
    bool map();

    /// Removes the existing mapping of the file, if any.
    void unmap();

    /// Returns the address where the memory is mapped.
    /// Assertion is triggered if there is no mapped memory.
    void* get_memory() { assert(m_memory); return m_memory; }

    /// Returns the underlying file descriptor.
    /// An assertion is triggered if there is no file descriptor.
    int get_fd() { assert(m_fd >= 0); return m_fd; }

    /// Returns the size of the shared memory, in bytes.
    size_t get_size() const { return m_size; }
};
