#pragma once

#include "matching/Order.hpp"
struct alignas(64) L2Data {
    uint8_t instrument_id_;
    uint8_t price_level_;
    int32_t quantity_;
    OrderSide side_;
};
