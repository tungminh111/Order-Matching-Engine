#pragma once

#include "matching/Order.hpp"
struct alignas(64) L2Data {
    int8_t instrument_id_;
    int8_t price_level_;
    int32_t quantity_;
    OrderSide side_;
};
