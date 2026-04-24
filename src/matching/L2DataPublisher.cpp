#include "matching/L2DataPublisher.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

#include "matching/sbe/L2Data.h"
#include "matching/sbe/MessageHeader.h"
#include "matching/sbe/SideEnum.h"

L2DataPublisher::L2DataPublisher(
    std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer)
    : l2_data_buffer_(l2_data_buffer) {}

void L2DataPublisher::start() {
    // setup multicast IP & enable loopback
    server_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("fail to create UDP socket");
    }

    int opt = 0;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&multicast_addr_, 0, sizeof(multicast_addr_));
    multicast_addr_.sin_family = AF_INET;
    multicast_addr_.sin_port = htons(49998);
    if (inet_pton(AF_INET, "239.255.0.1", &multicast_addr_.sin_addr) < 0) {
        throw std::runtime_error("fail to configure multicast addr");
    }

    struct sockaddr_in loopback_addr;
    memset(&loopback_addr, 0, sizeof(loopback_addr));
    if (inet_pton(AF_INET, "127.0.0.1", &loopback_addr.sin_addr) < 0) {
        throw std::runtime_error("fail to define loopback addr");
    }

    if (setsockopt(server_fd_, IPPROTO_IP, IP_MULTICAST_IF, &loopback_addr,
                   sizeof(loopback_addr)) < 0) {
        throw std::runtime_error("fail to configure loopback addr");
    }
    unsigned char loop = 1;
    if (setsockopt(server_fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
                   sizeof(loop)) < 0) {
        throw std::runtime_error("fail to enable loopback");
    }

    // proceed l2 data
    auto now = std::chrono::system_clock::now();
    int64_t sequence_id_count =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
    constexpr int buffer_size =
        sbe::L2Data::SBE_BLOCK_LENGTH + sbe::MessageHeader::encodedLength();
    char buffer[buffer_size];
    sbe::MessageHeader hdr;
    sbe::L2Data l2_data_wrapper;
    hdr.wrap(buffer, 0, 0, buffer_size);
    l2_data_wrapper.wrapForEncode(buffer, sbe::MessageHeader::encodedLength(),
                                  buffer_size);

    while (true) {
        if (!l2_data_buffer_->canRead()) continue;
        L2Data l2_data = l2_data_buffer_->read();
        l2_data_wrapper.instrumentId(l2_data.instrument_id_)
            .price_level(l2_data.price_level_)
            .quantity(l2_data.quantity_)
            .side(static_cast<sbe::SideEnum::Value>(l2_data.side_))
            .sequenceId(sequence_id_count++);

        auto err =
            sendto(server_fd_, buffer, buffer_size, 0,
                   (struct sockaddr*)&multicast_addr_, sizeof(multicast_addr_));
        if (err < 0) {
            throw std::runtime_error("UDP send err: " + std::to_string(err));
        }
    }
}

L2DataPublisher::~L2DataPublisher() {
    if (server_fd_ == -1) {
        close(server_fd_);
    }
}
