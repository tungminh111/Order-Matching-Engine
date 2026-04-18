#pragma once
#include <array>
#include <atomic>

template <class T, int capacity>
class SPSC {
   public:
    SPSC() {};
    void write(T order) {
        // assuming capacity never reached
        buffer_[last & (capacity - 1)] = order;
        last += 1;
    }

    T read() { return buffer_[first++]; }

    bool canRead() { return first < last; }

   private:
    std::array<T, capacity> buffer_;
    alignas(64) std::atomic<int> first{0}, last{0};
};
