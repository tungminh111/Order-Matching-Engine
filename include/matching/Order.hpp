#pragma once

#include <cstdint>

enum OrderSide { Buy = 0, Sell };

enum OrderType { Limit = 0, Market };

enum OrderAction { Create = 0, Modify, Cancel };

struct alignas(64) Order {
    uint8_t order_id_;
    uint8_t timestamp_;
    uint8_t price_;
    uint8_t quantity_;
    uint8_t instrument_id_;
    OrderSide side_;
    OrderType type_;
    OrderAction action_;

    bool operator==(const Order&) const = default;
};

struct alignas(64) MatchedOrder {
    MatchedOrder(int8_t order_id, int8_t quantity, bool match_full)
        : order_id_(order_id), quantity_(quantity), match_full_(match_full) {}
    int8_t order_id_;
    int8_t quantity_;
    bool match_full_;

    int64_t sequence_id_;
};
