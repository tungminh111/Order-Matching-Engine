#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
template <class T, size_t capacity>
class BytesBuffer {
   public:
    BytesBuffer();
    bool write(const char* buffer, size_t sz) {
        if (last - first + sz > capacity) return false;
        memcpy(buffer_.data() + (last & capacity - 1), buffer, sz);
        last += sz;
        return true;
    }
    T read() {
        T msg = *reinterpret_cast<T*>(&buffer_[first & (capacity - 1)]);
        first += sizeof(T);
        return msg;
    }

    bool canRead() { return first + sizeof(T) < last; }

   private:
    std::array<std::byte, capacity> buffer_;
    int64_t first, last;
};
