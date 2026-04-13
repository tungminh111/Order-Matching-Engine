#pragma once
#include <atomic>

#include "Order.hpp"

class OrderBuffer {
   public:
    OrderBuffer(int cap);
    void write(Order&& order);
    Order read();

   private:
    int cap;
    Order* buffer_;
    alignas(64) std::atomic<int> first, last;
};
