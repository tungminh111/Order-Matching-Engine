#pragma once
#include <array>
#include <atomic>

#include "Order.hpp"

template <int capacity>
class OrderBuffer {
   public:
    OrderBuffer() {};
    void write(Order&& order) {
        // assuming capacity never reached
        buffer_[last & (capacity - 1)] = order;
        last += 1;
    }
    Order read() { return buffer_[first++]; }

    bool canRead() { return first < last; }

   private:
    std::array<Order, capacity> buffer_;
    alignas(64) std::atomic<int> first{0}, last{0};
};
