#pragma once
#include <array>
#include <atomic>

template <class T, int capacity>
class SPSC {
   public:
    SPSC() {};
    void write(T order) {
        // assuming capacity never reached
        buffer_[last_ & (capacity - 1)] = order;
        last_ += 1;
    }

    T read() {
        T ret = buffer_[first_ & (capacity - 1)];
        first_ += 1;
        return ret;
    }

    bool canRead() { return first_ < last_; }

   private:
    std::array<T, capacity> buffer_;
    alignas(64) std::atomic<int> first_{0}, last_{0};
};
