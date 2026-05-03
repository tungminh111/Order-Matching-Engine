#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>

#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "matching/Order.hpp"
#include "matching/SPSC.hpp"
#include "matching/StreamConsumer.hpp"
#include "matching/sbe/ActionEnum.h"
#include "matching/sbe/MessageHeader.h"
#include "matching/sbe/NewOrder.h"
#include "matching/sbe/SideEnum.h"
#include "matching/sbe/TypeEnum.h"

class TestClient {
   public:
    TestClient() {
        buffer_ =
            static_cast<char*>(mmap(nullptr, 1 << 15, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(49999);

        if (connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("fail to connect to StreamConsumer");
        }

        sbe::MessageHeader hdr;
        hdr.wrap(buffer_, 0, 0, sbe::MessageHeader::encodedLength())
            .blockLength(sbe::NewOrder::sbeBlockLength())
            .templateId(sbe::NewOrder::sbeTemplateId())
            .schemaId(sbe::NewOrder::sbeSchemaId())
            .version(sbe::NewOrder::sbeSchemaVersion());
        order_msg_.wrapForEncode(buffer_, sbe::MessageHeader::encodedLength(),
                                 sbe::MessageHeader::encodedLength() +
                                     sbe::NewOrder::SBE_BLOCK_LENGTH);
    };

    void sendOrder(const Order& order) {
        order_msg_.orderId(order.order_id_)
            .timestamp(order.timestamp_)
            .price(order.price_)
            .quantity(order.quantity_)
            .instrumentId(order.instrument_id_)
            .side(static_cast<sbe::SideEnum::Value>(order.side_))
            .type(static_cast<sbe::TypeEnum::Value>(order.type_))
            .action(static_cast<sbe::ActionEnum::Value>(order.action_));

        send(socket_fd_, buffer_,
             sbe::MessageHeader::encodedLength() +
                 sbe::NewOrder::SBE_BLOCK_LENGTH,
             0);
    }

   private:
    int socket_fd_;
    char* buffer_;
    sbe::NewOrder order_msg_;
};

TEST(StreamConsumerTest, HappyCase) {
    std::shared_ptr<DefaultSPSC<Order>> order_buffer =
        std::make_shared<DefaultSPSC<Order>>();

    StreamConsumer consumer(order_buffer);
    std::thread consumer_thread(&StreamConsumer::start, &consumer);

    TestClient producer;

    std::vector<Order> orders = {{.order_id_ = 1,
                                  .timestamp_ = 2,
                                  .price_ = 3,
                                  .quantity_ = 4,
                                  .instrument_id_ = 5,
                                  .side_ = OrderSide::Buy,
                                  .type_ = OrderType::Limit,
                                  .action_ = OrderAction::Create},
                                 {.order_id_ = 11,
                                  .timestamp_ = 22,
                                  .price_ = 33,
                                  .quantity_ = 44,
                                  .instrument_id_ = 55,
                                  .side_ = OrderSide::Sell,
                                  .type_ = OrderType::Market,
                                  .action_ = OrderAction::Cancel}};
    for (auto& order : orders) producer.sendOrder(order);

    std::vector<Order> result_orders;
    while (result_orders.size() < orders.size()) {
        if (!order_buffer->canRead()) continue;

        result_orders.push_back(order_buffer->read());
    }

    for (int i = 0; i < result_orders.size(); ++i) {
        ASSERT_EQ(result_orders[i], orders[i]);
    }

    consumer.stop();
    consumer_thread.join();
}

