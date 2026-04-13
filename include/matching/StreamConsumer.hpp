#pragma once

#include "BytesBuffer.hpp"
#include "Order.hpp"
#include "matching/OrderBuffer.hpp"
#include <memory>
class StreamConsumer {
public:
    StreamConsumer(std::shared_ptr<OrderBuffer> order_buffer);
    void start();
private: 

    ~StreamConsumer();
    std::shared_ptr<OrderBuffer> order_buffer_;
    int server_fd_;
    int client_fd_;
};
