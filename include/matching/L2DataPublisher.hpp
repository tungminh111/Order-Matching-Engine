#pragma once

#include <memory>

#include <netinet/in.h>
#include "matching/L2Data.hpp"
#include "matching/SPSC.hpp"

class L2DataPublisher {
   public:
    L2DataPublisher(std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer);
    ~L2DataPublisher();

    void start();

   private:
    std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer_;
    int server_fd_ = -1;
    struct sockaddr_in multicast_addr_;
};
