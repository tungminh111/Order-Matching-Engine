#pragma once

#include <memory>

#include "matching/Order.hpp"
#include "matching/SPSC.hpp"
class StreamConsumer {
   public:
    StreamConsumer(std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer);
    ~StreamConsumer();

    void start();

   private:
    std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer_;
    int server_fd_ = -1;
    int client_fd_ =-1;
};
