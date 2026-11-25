#include <gtest/gtest.h>

#include <cstdlib>
#include <ios>
#include <iterator>
#include <memory>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

#include "matching/L2Data.hpp"
#include "matching/MetricsRecorder.hpp"
#include "matching/Order.hpp"
#include "matching/OrderMatcher.hpp"
#include "matching/SPSC.hpp"

void TestOrderMatcherSingleCase(std::vector<Order> orders,
                                std::vector<L2Data> expect_l2s,
                                std::vector<MatchedOrder> expect_matches

) {
    auto input_order_buffer = std::make_shared<DefaultSPSC<Order>>();
    auto l2_buffer = std::make_shared<DefaultSPSC<L2Data>>();
    auto match_order_buffer = std::make_shared<DefaultSPSC<MatchedOrder>>();

    auto order_producer = [&]() {
        for (auto order : orders) {
            input_order_buffer->write(order);
        }
    };

    std::thread order_producer_thread(order_producer);

    auto metrics_buffer = std::make_shared<DefaultSPSC<int64_t>>();
    auto metrics_recorder = std::make_shared<MetricsRecorder>(metrics_buffer);

    OrderMatcher order_matcher(input_order_buffer, l2_buffer,
                               match_order_buffer, metrics_recorder);
    std::thread order_matcher_thread(&OrderMatcher::start, &order_matcher);

    std::vector<L2Data> result_l2_data;
    std::vector<MatchedOrder> result_match_orders;

    while (result_l2_data.size() != expect_l2s.size() ||
           result_match_orders.size() != expect_matches.size()) {
        if (l2_buffer->canRead()) result_l2_data.push_back(l2_buffer->read());
        if (match_order_buffer->canRead())
            result_match_orders.push_back(match_order_buffer->read());
    }

    order_matcher.stop();
    order_matcher_thread.join();
    order_producer_thread.join();

    for (int i = 0; i < expect_l2s.size(); ++i) {
        ASSERT_EQ(expect_l2s[i], result_l2_data[i]);
    }
    for (int i = 0; i < expect_matches.size(); ++i) {
        ASSERT_EQ(expect_matches[i], result_match_orders[i]);
    }
}

TEST(OrderMatcherTest, MarketOrderPartialFilled) {
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
            .price_level_ = 150,
            .quantity_ = -100,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 151,
            .quantity_ = -50,
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

    TestOrderMatcherSingleCase(orders, expect_l2s, expect_matches);
}

TEST(OrderMatcherTest, ModifyOrder) {
    std::vector<Order> orders = {
        {
            .order_id_ = 1,
            .price_ = 150,
            .quantity_ = 100,
            .side_ = OrderSide::Buy,
            .type_ = OrderType::Limit,
            .action_ = OrderAction::Create,
        },
        {
            .order_id_ = 2,
            .price_ = 150,
            .quantity_ = 100,
            .side_ = OrderSide::Buy,
            .type_ = OrderType::Limit,
            .action_ = OrderAction::Create,
        },
        {
            .order_id_ = 1,
            .quantity_ = 50,
            .action_ = OrderAction::Modify,
        },
        {
            .order_id_ = 2,
            .quantity_ = 150,
            .action_ = OrderAction::Modify,
        },
        {
            .order_id_ = 3,
            .quantity_ = 200,
            .side_ = OrderSide::Sell,
            .type_ = OrderType::Market,
            .action_ = OrderAction::Create,
        },
    };

    std::vector<L2Data> expect_l2s = {
        {
            .price_level_ = 150,
            .quantity_ = 100,
            .side_ = OrderSide::Buy,
        },
        {
            .price_level_ = 150,
            .quantity_ = 100,
            .side_ = OrderSide::Buy,
        },
        {
            .price_level_ = 150,
            .quantity_ = -50,
            .side_ = OrderSide::Buy,
        },
        {
            .price_level_ = 150,
            .quantity_ = -100,
            .side_ = OrderSide::Buy,
        },
        {
            .price_level_ = 150,
            .quantity_ = 150,
            .side_ = OrderSide::Buy,
        },
        {
            .price_level_ = 150,
            .quantity_ = -200,
            .side_ = OrderSide::Buy,
        },
    };

    std::vector<MatchedOrder> expect_matches = {
        {
            .order_id_ = 1,
            .quantity_ = 50,
            .match_full_ = true,
        },
        {
            .order_id_ = 3,
            .quantity_ = 50,
            .match_full_ = false,
        },
        {
            .order_id_ = 2,
            .quantity_ = 150,
            .match_full_ = true,
        },
        {
            .order_id_ = 3,
            .quantity_ = 150,
            .match_full_ = true,
        },
    };

    TestOrderMatcherSingleCase(orders, expect_l2s, expect_matches);
}

TEST(OrderMatcherTest, GhostOrder) {
    std::vector<Order> orders = {
        {
            .order_id_ = 1,
            .price_ = 150,
            .quantity_ = 100,
            .side_ = OrderSide::Sell,
            .type_ = OrderType::Limit,
            .action_ = OrderAction::Create,
        },
        {
            .order_id_ = 2,
            .price_ = 151,
            .quantity_ = 100,
            .side_ = OrderSide::Sell,
            .type_ = OrderType::Limit,
            .action_ = OrderAction::Create,
        },
        {
            .order_id_ = 3,
            .price_ = 152,
            .quantity_ = 100,
            .side_ = OrderSide::Sell,
            .type_ = OrderType::Limit,
            .action_ = OrderAction::Create,
        },
        {
            .order_id_ = 2,
            .action_ = OrderAction::Cancel,
        },
        {
            .order_id_ = 4,
            .price_ = 152,
            .quantity_ = 250,
            .side_ = OrderSide::Buy,
            .type_ = OrderType::Limit,
            .action_ = OrderAction::Create,
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
            .quantity_ = 100,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 152,
            .quantity_ = 100,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 151,
            .quantity_ = -100,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 150,
            .quantity_ = -100,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 152,
            .quantity_ = -100,
            .side_ = OrderSide::Sell,
        },
        {
            .price_level_ = 152,
            .quantity_ = 50,
            .side_ = OrderSide::Buy,
        },
    };

    std::vector<MatchedOrder> expect_matches = {
        {
            .order_id_ = 1,
            .quantity_ = 100,
            .match_full_ = true,
        },
        {
            .order_id_ = 4,
            .quantity_ = 100,
            .match_full_ = false,
        },
        {
            .order_id_ = 3,
            .quantity_ = 100,
            .match_full_ = true,
        },
        {
            .order_id_ = 4,
            .quantity_ = 100,
            .match_full_ = false,
        },
    };

    TestOrderMatcherSingleCase(orders, expect_l2s, expect_matches);
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

uint64_t id_counter = 0;

Order newOrder(OrderSide side, uint64_t price, uint64_t qty) {
    return Order{.order_id_ = id_counter++,
                 .price_ = price,
                 .quantity_ = qty,
                 .side_ = side};
}

template <typename T, T min_rand, T max_rand>
T randInt() {
    static_assert(min_rand < max_rand);
    T range = max_rand - min_rand;
    return min_rand + ((uint64_t)rand() * rand() * rand()) % range;
}

std::pair<int64_t, std::vector<Order>> randomOrder(OrderSide side,
                                                   uint64_t price_level) {
    size_t level_size = randInt<size_t, 1, 10>();
    uint64_t sum = 0;
    std::vector<Order> orders;
    for (int i = 0; i < level_size; ++i) {
        uint64_t qty = randInt<uint64_t, 1, 10000>();
        orders.push_back(newOrder(side, price_level, qty));
    }
    return {sum, orders};
}

TEST(OrderMatcherTest, PerformanceTest) {
    // random mid price
    // choose next 1000000 levels, random 10 orders with 10 price
    srand(time(nullptr));
    const int iteration = 30;
    const int book_depth = 1e5;

    uint64_t prev_mid_price = -1;
    std::multiset<std::pair<uint64_t, std::pair<uint64_t, Order>>> asks;
    std::multiset<std::pair<uint64_t, std::pair<int64_t, Order>>,
                  std::greater<>>
        bids;

    auto input_order_buffer = std::make_shared<DefaultSPSC<Order>>();
    auto l2_buffer = std::make_shared<DefaultSPSC<L2Data>>();
    auto match_order_buffer = std::make_shared<DefaultSPSC<MatchedOrder>>();

    // INIT DEPENDENCIES
    std::vector<L2Data> expect_l2s;
    std::vector<L2Data> result_l2_data;
    std::vector<MatchedOrder> expect_matches;
    std::vector<MatchedOrder> result_match_orders;
    bool l2_consumer_stop = false;
    auto l2_consumer = [&]() {
        while (!l2_consumer_stop ||
               (l2_buffer->canRead() || match_order_buffer->canRead())) {
            if (l2_buffer->canRead())
                result_l2_data.push_back(l2_buffer->read());
            if (match_order_buffer->canRead())
                result_match_orders.push_back(match_order_buffer->read());
        }
    };
    std::thread result_verifier_thread(l2_consumer);

    auto metrics_buffer = std::make_shared<DefaultSPSC<int64_t>>();
    auto metrics_recorder = std::make_shared<MetricsRecorder>(metrics_buffer);

    std::vector<int64_t> latencies;
    bool metrics_consumer_stop = false;
    auto metrics_consumer = [&]() {
        while (!metrics_consumer_stop || metrics_buffer->canRead()) {
            if (metrics_buffer->canRead())
                latencies.push_back(metrics_buffer->read());
        }
    };
    std::thread metrics_consumer_thread(metrics_consumer);

    OrderMatcher order_matcher(input_order_buffer, l2_buffer,
                               match_order_buffer, metrics_recorder);
    std::thread order_matcher_thread(&OrderMatcher::start, &order_matcher);
    std::map<uint64_t, Order> order_map;

    // PROCESS

    for (int i = 0; i < iteration; ++i) {
        //uint64_t mid_price = i * 2000;
        uint64_t mid_price = randInt<uint64_t, 1000000, 9000000>();
        std::vector<Order> orders;

        // move mid price
        if (prev_mid_price != -1) {
            const uint64_t max_single_order_qty = 20000;
            int64_t cur_qty = 0;
            uint64_t last_order_price = 0;
            int64_t last_price_qty = 0;
            if (mid_price > prev_mid_price) {
                while (!asks.empty() && asks.begin()->first <= mid_price) {
                    auto maker_order = asks.begin()->second.second;
                    asks.erase(asks.begin());

                    if (maker_order.price_ != last_order_price &&
                        last_order_price != 0 && last_price_qty != 0) {
                        expect_l2s.push_back(
                            L2Data{.price_level_ = last_order_price,
                                   .quantity_ = -last_price_qty,
                                   .side_ = OrderSide::Sell});
                        last_price_qty = maker_order.quantity_;
                    } else {
                        last_price_qty += maker_order.quantity_;
                    }
                    last_order_price = maker_order.price_;

                    cur_qty += maker_order.quantity_;
                    bool flush =
                        (asks.empty() || asks.begin()->first > mid_price) || (cur_qty >= max_single_order_qty);

                    expect_matches.push_back(
                        MatchedOrder{.order_id_ = maker_order.order_id_,
                                     .quantity_ = maker_order.quantity_,
                                     .match_full_ = true});
                    expect_matches.push_back(
                        MatchedOrder{.order_id_ = id_counter,
                                     .quantity_ = maker_order.quantity_,
                                     .match_full_ = flush});

                    if (flush) {
                        orders.push_back(
                            newOrder(OrderSide::Buy, mid_price, cur_qty));
                        cur_qty = 0;

                        expect_l2s.push_back(
                            L2Data{.price_level_ = last_order_price,
                                   .quantity_ = -last_price_qty,
                                   .side_ = OrderSide::Sell});
                        last_price_qty = 0;
                    }
                }
                if (last_price_qty != 0) {
                    expect_l2s.push_back(
                        L2Data{.price_level_ = last_order_price,
                               .quantity_ = -last_price_qty,
                               .side_ = OrderSide::Sell});
                }
            } else {
                while (!bids.empty() && bids.begin()->first > mid_price) {
                    auto maker_order = bids.begin()->second.second;
                    bids.erase(bids.begin());

                    if (maker_order.price_ != last_order_price &&
                        last_order_price != 0 && last_price_qty != 0) {
                        expect_l2s.push_back(
                            L2Data{.price_level_ = last_order_price,
                                   .quantity_ = -last_price_qty,
                                   .side_ = OrderSide::Buy});
                        last_price_qty = maker_order.quantity_;
                    } else {
                        last_price_qty += maker_order.quantity_;
                    }
                    last_order_price = maker_order.price_;


                    cur_qty += maker_order.quantity_;
                    bool flush =
                        (bids.empty() || bids.begin()->first <= mid_price) || (cur_qty >= max_single_order_qty);
                    expect_matches.push_back(
                        MatchedOrder{.order_id_ = maker_order.order_id_,
                                     .quantity_ = maker_order.quantity_,
                                     .match_full_ = true});
                    expect_matches.push_back(
                        MatchedOrder{.order_id_ = id_counter,
                                     .quantity_ = maker_order.quantity_,
                                     .match_full_ = flush});
                    if (flush) {
                        orders.push_back(
                            newOrder(OrderSide::Sell, mid_price + 1, cur_qty));
                        cur_qty = 0;

                        expect_l2s.push_back(
                            L2Data{.price_level_ = last_order_price,
                                   .quantity_ = -last_price_qty,
                                   .side_ = OrderSide::Buy});
                        last_price_qty = 0;
                    }
                }
                if (last_price_qty != 0) {
                    expect_l2s.push_back(
                        L2Data{.price_level_ = last_order_price,
                               .quantity_ = -last_price_qty,
                               .side_ = OrderSide::Buy});
                }
            }
        }

        // place move orders
        for (auto& order : orders) {
            while (!input_order_buffer->canWrite()) {
            }
            input_order_buffer->write(order);
            order_map[order.order_id_] = order;
        }
        orders.clear();

        // make new orders
        for (auto j = mid_price; j > mid_price - book_depth; --j) {
            auto [level_size, level_orders] = randomOrder(OrderSide::Buy, j);
            for (auto& order : level_orders) {
                orders.push_back(order);
                bids.emplace(j, std::make_pair(-order.order_id_, order));
                expect_l2s.push_back(
                    L2Data{.price_level_ = j,
                           .quantity_ = static_cast<int64_t>(order.quantity_),
                           .side_ = OrderSide::Buy});
            }
        }
        for (auto j = mid_price + 1; j <= mid_price + book_depth; ++j) {
            auto [level_size, level_orders] = randomOrder(OrderSide::Sell, j);
            for (auto& order : level_orders) {
                orders.push_back(order);
                asks.emplace(j, std::make_pair(order.order_id_, order));
                expect_l2s.push_back(
                    L2Data{.price_level_ = j,
                           .quantity_ = static_cast<int64_t>(order.quantity_),
                           .side_ = OrderSide::Sell});
            }
        }

        // place new orders
        for (auto& order : orders) {
            while (!input_order_buffer->canWrite()) {
            }
            input_order_buffer->write(order);
            order_map[order.order_id_] = order;
        }
        orders.clear();

        prev_mid_price = mid_price;
    }

    order_matcher.stop();
    order_matcher_thread.join();

    l2_consumer_stop = true;
    result_verifier_thread.join();

    metrics_consumer_stop = true;
    metrics_consumer_thread.join();

    // ASSERT_EQ(expect_matches.size(), result_match_orders.size());
    for (int i = 0; i < expect_matches.size(); ++i) {
        auto x = expect_matches[i];
        auto y = result_match_orders[i];
        assert(x == y);
        assert(order_map[x.order_id_].order_id_ == x.order_id_);
        ASSERT_EQ(expect_matches[i], result_match_orders[i]);
    }
    ASSERT_EQ(expect_l2s.size(), result_l2_data.size());
    for (int i = 0; i < expect_l2s.size(); ++i) {
        ASSERT_EQ(expect_l2s[i], result_l2_data[i]);
    }

    int64_t sum_latencies = std::accumulate(latencies.begin(), latencies.end(), 0LL);
    int64_t mean = sum_latencies / latencies.size();
    std::cout << "Latency: " << mean << "ns" << std::endl;
    std::cout << "Sample count: " << latencies.size() << std::endl;

}
