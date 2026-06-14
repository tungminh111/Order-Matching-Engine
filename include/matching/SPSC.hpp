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
        last_.fetch_add(1, std::memory_order_release);
    }

    T read() {
        T ret = buffer_[first_ & (capacity - 1)];
        first_.fetch_add(1, std::memory_order_release);
        return ret;
    }

    bool canRead() {
        return first_.load(std::memory_order_relaxed) <
               last_.load(std::memory_order_acquire);
    }

   private:
    std::array<T, capacity> buffer_;
    alignas(64) std::atomic<int> first_{0}, last_{0};
};

template <class T>
using DefaultSPSC = SPSC<T, 1 << 15>;
