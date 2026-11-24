#pragma once
#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>

template <class T, int capacity>
class SPSC {
   public:
    SPSC() {};
    void write(T order) {
        while (!canWrite()) {
            throw std::runtime_error("Queue full");
            std::cout << "Queue full" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        int l = last_.load(std::memory_order_relaxed);
        buffer_[l & (capacity - 1)] = order;
        last_.store(l + 1, std::memory_order_release);
    }

    T read() {
        int f = first_.load(std::memory_order_relaxed);
        T ret = buffer_[f & (capacity - 1)];
        buffer_[f & (capacity - 1)] = T();
        first_.store(f + 1, std::memory_order_release);
        return ret;
    }

    bool canRead() {
        return first_.load(std::memory_order_relaxed) !=
               last_.load(std::memory_order_acquire);
    }

    bool canWrite() {
        return last_.load(std::memory_order_relaxed) -
                   first_.load(std::memory_order_acquire) <
               capacity;
    }

   private:
    std::array<T, capacity> buffer_;
    alignas(64) std::atomic<uint64_t> first_{0}, last_{0};
};

template <class T>
using DefaultSPSC = SPSC<T, 1 << 15>;
