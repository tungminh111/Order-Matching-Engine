#pragma once

#include "matching/Order.hpp"
struct alignas(64) L2Data {
    uint8_t instrument_id_;
    uint64_t price_level_;
    int64_t quantity_;
    OrderSide side_;

    bool operator==(const L2Data&) const = default;
};
