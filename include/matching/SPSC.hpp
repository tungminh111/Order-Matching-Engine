#pragma once
#include <array>
#include <atomic>
#include <cassert>

template <class T, int capacity>
class SPSC {
   public:
    SPSC() {};
    void write(T order) {
        // assuming capacity never reached
        int l = last_.load(std::memory_order_relaxed);
        assert(l - first_.load(std::memory_order_acquire) > capacity);
        buffer_[l & (capacity - 1)] = order;
        last_.store(l + 1, std::memory_order_release);
    }

    T read() {
        int f = first_.load(std::memory_order_relaxed);
        T ret =
            buffer_[f & (capacity - 1)];
        first_.store(f + 1, std::memory_order_release);
        return ret;
    }

    bool canRead() {
        return first_.load(std::memory_order_relaxed) !=
               last_.load(std::memory_order_acquire);
    }

   private:
    std::array<T, capacity> buffer_;
    alignas(64) std::atomic<uint32_t> first_{0}, last_{0};
};

template <class T>
using DefaultSPSC = SPSC<T, 1 << 15>;
