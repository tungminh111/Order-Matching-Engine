#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <vector>

#include "matching/L2Data.hpp"
#include "matching/Order.hpp"
#include "matching/OrderMatcher.hpp"
#include "matching/SPSC.hpp"

TEST(OrderMatcherTest, HappyCase) {
    std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer =
        std::make_shared<SPSC<Order, 1 << 15>>();

    std::vector<Order> orders = {
        {
            .order_id_ = 1,
            .price_ = 150,
            .quantity_ = 100,
            .side_ = OrderSide::Sell,
            .type_ = OrderType::Limit,
        },
        {
            .order_id_ = 2,
            .price_ = 151,
            .quantity_ = 50,
            .side_ = OrderSide::Sell,
            .type_ = OrderType::Limit,
        },
        {
            .order_id_ = 3,
            .quantity_ = 200,
            .side_ = OrderSide::Buy,
            .type_ = OrderType::Market,
        },
    };

    std::vector<L2Data> expect_l2s = {
        {
            .price_level_ = 150,
            .quantity_ = 100,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 151,
            .quantity_ = 50,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 151,
            .quantity_ = -50,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 150,
            .quantity_ = -100,
            .side_ = OrderSide::Sell,
        },
    };

    std::vector<MatchedOrder> expect_matches = {
        {
            .order_id_ = 1,
            .quantity_ = 100,
            .match_full_ = true,
        },
        {
            .order_id_ = 3,
            .quantity_ = 100,
            .match_full_ = false,
        },
        {
            .order_id_ = 2,
            .quantity_ = 50,
            .match_full_ = true,
        },
        {
            .order_id_ = 3,
            .quantity_ = 50,
            .match_full_ = false,
        },
    };

    auto input_order_buffer = std::make_shared<DefaultSPSC<Order>>();
    auto l2_buffer = std::make_shared<DefaultSPSC<L2Data>>();
    auto match_order_buffer = std::make_shared<DefaultSPSC<MatchedOrder>>();

    auto order_producer = [&]() {
        for (auto order : orders) {
            input_order_buffer->write(order);
        }
    };

    std::thread order_producer_thread(order_producer);

    OrderMatcher order_matcher(input_order_buffer, l2_buffer,
                               match_order_buffer);
    std::thread order_matcher_thread(&OrderMatcher::start, &order_matcher);

    std::vector<L2Data> result_l2_data;
    std::vector<MatchedOrder> result_match_orders;

    ASSERT_EQ(expect_l2s.size(), result_l2_data.size());
    ASSERT_EQ(expect_matches.size(), result_match_orders.size());
    for (int i = 0; i < expect_l2s.size(); ++i) {
        ASSERT_EQ(expect_l2s[i], result_l2_data[i]);
    }
    for (int i = 0; i < expect_matches.size(); ++i) {
        ASSERT_EQ(expect_matches[i], result_match_orders[i]);
    }
}

/**
 *
Test Case 1: The Market Order "Fill and Kill"
INPUT list

Add Order 1: LIMIT SELL 100 @ $150

Add Order 2: LIMIT SELL 50 @ $151

Fire Order 3: MARKET BUY 200

EXPECT L2 DELTAS list

ADD | ASK | $150.00 | Qty: 100 (Triggered by Input 1)

ADD | ASK | $151.00 | Qty: 50 (Triggered by Input 2)

DELETE | ASK | $150.00 (Triggered by Input 3)

DELETE | ASK | $151.00 (Triggered by Input 3)

EXPECT match order list

Match 1:

Order IDs: 3 (Buyer) and 1 (Seller)

Match amount: 100

Is matched fully: Order 1 (Yes), Order 3 (No)

Match 2:

Order IDs: 3 (Buyer) and 2 (Seller)

Match amount: 50

Is matched fully: Order 2 (Yes), Order 3 (No — remaining 50 is dropped)

Test Case 2: The "Modify Queue" Trap
INPUT list

Add Order 1: LIMIT BUY 100 @ $150

Add Order 2: LIMIT BUY 100 @ $150

Modify Order 1: Change Qty to 50

Modify Order 2: Change Qty to 150

Fire Order 5: MARKET SELL 100

EXPECT L2 DELTAS list

ADD | BID | $150.00 | Qty: 100 (Triggered by Input 1)

MODIFY | BID | $150.00 | Qty: 200 (Triggered by Input 2)

MODIFY | BID | $150.00 | Qty: 150 (Triggered by Input 3)

MODIFY | BID | $150.00 | Qty: 200 (Triggered by Input 4)

MODIFY | BID | $150.00 | Qty: 100 (Triggered by Input 5)

EXPECT match order list

Match 1:

Order IDs: 5 (Seller) and 1 (Buyer)

Match amount: 50

Is matched fully: Order 1 (Yes), Order 5 (No)

Match 2:

Order IDs: 5 (Seller) and 2 (Buyer)

Match amount: 50

Is matched fully: Order 5 (Yes), Order 2 (No)

Test Case 3: The "Ghost Order" (Cancel & Gap)
INPUT list

Add Order 1: LIMIT SELL 100 @ $150

Add Order 2: LIMIT SELL 100 @ $151

Add Order 3: LIMIT SELL 100 @ $152

Cancel Order 2

Fire Order 5: MARKET BUY 150

EXPECT L2 DELTAS list

ADD | ASK | $150.00 | Qty: 100 (Triggered by Input 1)

ADD | ASK | $151.00 | Qty: 100 (Triggered by Input 2)

ADD | ASK | $152.00 | Qty: 100 (Triggered by Input 3)

DELETE | ASK | $151.00 (Triggered by Input 4)

DELETE | ASK | $150.00 (Triggered by Input 5)

MODIFY | ASK | $152.00 | Qty: 50 (Triggered by Input 5)

EXPECT match order list

Match 1:

Order IDs: 5 (Buyer) and 1 (Seller)

Match amount: 100

Is matched fully: Order 1 (Yes), Order 5 (No)

Match 2:

Order IDs: 5 (Buyer) and 3 (Seller)

Match amount: 50

Is matched fully: Order 5 (Yes), Order 3 (No)
**/
