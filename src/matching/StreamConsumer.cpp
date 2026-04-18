#include "matching/StreamConsumer.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

#include "matching/BytesBuffer.hpp"
#include "matching/Order.hpp"

StreamConsumer::StreamConsumer(
    std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer)
    : order_buffer_(order_buffer) {}

void StreamConsumer::start() {
    BytesBuffer<Order, 1 << 15> byte_buffer;

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("fail to create TCP socket");
    }

    int opt = 0;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(49999);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        throw std::runtime_error("fail to bind socket");
    }

    if (listen(server_fd_, 1) < 0) {
        throw std::runtime_error("socket fail to listen");
    }

    socklen_t addr_sz = sizeof(addr);

    client_fd_ = accept(server_fd_, (struct sockaddr*)&addr, &addr_sz);
    if (client_fd_ < 0) {
        throw std::runtime_error("fail to accept connection");
    }

    int flag = 1;
    setsockopt(client_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    std::cout << "StreamConsumer start successfully" << std::endl;

    fcntl(client_fd_, F_SETFL, O_NONBLOCK);
    char buffer[1 << 15];
    while (true) {
        ssize_t bytes = recv(client_fd_, buffer, sizeof(buffer), 0);
        if (bytes > 0) {
            byte_buffer.write(buffer, bytes);
            while (byte_buffer.canRead()) {
                order_buffer_->write(byte_buffer.read());
            }
        } else {
            continue;
        }
    }
}

StreamConsumer::~StreamConsumer() {
    if (server_fd_ != -1) close(server_fd_);
    if (client_fd_ != -1) close(client_fd_);
    std::cout << "StreamConsumer stop successfully" << std::endl;
}
