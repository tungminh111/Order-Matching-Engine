#pragma once

#include <cstdint>

enum OrderSide { Buy, Sell };

enum OrderType { Limit, Market };

enum OrderAction { Create, Modify, Cancel };

struct alignas(64) Order {
    int8_t order_id_;
    int8_t timestamp_;
    int8_t price_;
    int8_t quantity_;
    int8_t instrument_id_;
    OrderSide side_;
    OrderType type_;
    OrderAction action_;
};

struct alignas(64) MatchedOrder {
    MatchedOrder(int8_t order_id, int8_t quantity, bool match_full)
        : order_id_(order_id), quantity_(quantity), match_full_(match_full) {}
    int8_t order_id_;
    int8_t quantity_;
    bool match_full_;

    int8_t sequence_id_;
};
