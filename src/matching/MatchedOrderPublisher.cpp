#include "matching/MatchedOrderPublisher.hpp"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>

#include "matching/Order.hpp"
#include "matching/sbe/MatchedOrder.h"
#include "matching/sbe/MessageHeader.h"

MatchedOrderPublisher::MatchedOrderPublisher(
    std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer)
    : matched_order_buffer_(matched_order_buffer) {
    client_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd_ < 0) {
        throw std::runtime_error(
            "fail to create matched order publisher socket");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(49999);

    if (connect(client_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error(
            "fail to create matched order publisher socket");
    }

    int flag = 1;
    setsockopt(client_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

MatchedOrderPublisher::~MatchedOrderPublisher() {
    if (client_fd_ >= 0) close(client_fd_);
}

void MatchedOrderPublisher::start() {
    constexpr int buffer_size = sbe::MatchedOrder::SBE_BLOCK_LENGTH +
                                sbe::MessageHeader::encodedLength();
    char buffer[buffer_size];
    sbe::MessageHeader hdr;
    sbe::MatchedOrder matched_order_wrapper;
    hdr.wrap(buffer, 0, 0, buffer_size);
    while (!stopped) {
        if (!matched_order_buffer_->canRead()) continue;
        MatchedOrder order = matched_order_buffer_->read();
        matched_order_wrapper.orderId(order.order_id_)
            .quantity(order.quantity_);
        send(client_fd_, buffer, buffer_size, 0);
    }
}

void MatchedOrderPublisher::stop() { stopped = true; }
