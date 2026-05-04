#pragma once

#include <netinet/in.h>

#include <memory>

#include "matching/Order.hpp"
#include "matching/SPSC.hpp"

class MatchedOrderPublisher {
   public:
    MatchedOrderPublisher(
        std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer);
    ~MatchedOrderPublisher();

    void start();
    void stop();

   private:
    std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer_;
    int client_fd_ = -1;
    bool stopped = false;
};
