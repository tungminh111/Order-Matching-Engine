#pragma once

#include <cstdint>

enum OrderSide {
    Buy,
    Sell
};

enum OrderType {
    Limit,
    Market
};

enum OrderAction {
    Create,
    Modify,
    Cancel
};

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
