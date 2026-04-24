#pragma once
#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

template <class T, size_t T_size, size_t capacity>
class BytesBuffer {
   public:
    BytesBuffer() {
        int page_size = _SC_PAGE_SIZE;
        static_assert(capacity % page_size == 0);

        int fd = shm_open("/byte_buffer", O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd == -1) {
            throw std::runtime_error(
                "fail to create phisical fd for byte buffer");
        }

        shm_unlink("/byte_buffer");

        if (ftruncate(fd, capacity) != 0) {
            throw std::runtime_error(
                "fail to allocate phisical mem for byte buffer");
        }

        buffer_ = mmap(nullptr, capacity * 2, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (buffer_ == MAP_FAILED) {
            close(fd);
            throw std::runtime_error(
                "fail to allocate virtual mem for byte buffer");
        }
        void* first_buff = mmap(buffer_, capacity, PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_FIXED, fd, 0);
        void* second_buff_addr = static_cast<char*>(first_buff) + capacity;
        void* second_buff =
            mmap(second_buff_addr, capacity, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FIXED, fd, 0);
        if (first_buff == MAP_FAILED || second_buff == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("fail to map virtual mem for byte buffer");
        }

        close(fd);
    }

    ~BytesBuffer() { munmap(buffer_, capacity * 2); }

    bool write(const char* buffer, size_t sz) {
        if (last - first + sz > capacity) return false;
        memcpy(static_cast<char*>(buffer_) + (last & capacity - 1), buffer, sz);
        last += sz;
        return true;
    }

    char* front() {
        return static_cast<char*>(buffer_) + (first & (capacity - 1));
    }

    void pop() { first += T_size; }

    bool canRead() { return first + T_size <= last; }

    const static size_t CAPACITY = capacity * 2;

   private:
    void* buffer_;

    int64_t first, last;
};
