#pragma once

#include <memory>

#include "matching/OrderBuffer.hpp"
class StreamConsumer {
   public:
    StreamConsumer(std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer);
    void start();

   private:
    ~StreamConsumer();
    std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer_;
    int server_fd_;
    int client_fd_;
};
