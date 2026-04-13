#pragma once 

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
template <class T, size_t capacity>
class BytesBuffer{
public:
    BytesBuffer();
    bool write(const char * buffer, size_t sz) {
        if (last - first + sz > capacity) 
            return false;
        memcpy(buffer_.data() + last, buffer, sz);
        last += sz;
        return true;
    }
    T* read() {
       if (last + sizeof(T) >= last) return nullptr;
       T* msg = reinterpret_cast<T*>(&buffer_[first & (capacity - 1)]);
       first += sizeof(T);
       return msg;
    }
private:
    std::array<std::byte, capacity> buffer_;
    int64_t first, last;
};
