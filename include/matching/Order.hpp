#pragma once

#include <cstdint>

enum OrderSide { Buy = 0, Sell };

enum OrderType { Limit = 0, Market };

enum OrderAction { Create = 0, Modify, Cancel };

struct alignas(64) Order {
    uint64_t order_id_;
    uint64_t timestamp_;
    uint64_t price_;
    uint64_t quantity_;
    uint8_t instrument_id_;
    OrderSide side_;
    OrderType type_;
    OrderAction action_;

    bool operator==(const Order&) const = default;
};

struct alignas(64) MatchedOrder {
    uint64_t order_id_;
    uint64_t quantity_;
    bool match_full_;

    uint64_t sequence_id_;

    bool operator==(const MatchedOrder&) const = default;
};
