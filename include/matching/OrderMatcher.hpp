#pragma once

#include <list>
#include <map>
#include <memory>
#include <unordered_map>

#include "matching/L2Data.hpp"
#include "matching/Order.hpp"
#include "matching/OrderBuffer.hpp"
class InstrumentOrderMatcher {
   public:
    InstrumentOrderMatcher(
        std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer,
        std::shared_ptr<SPSC<MatchedOrder, 1 << 15>> matched_order_buffer);

    void handleOrder(Order new_order);

    class PriceLevel {
       public:
        std::vector<MatchedOrder> matchOrder(Order& new_order);

        std::list<Order>::iterator push(Order new_order);

        void erase(std::list<Order>::iterator order_pos);

        bool isEmpty();

       private:
        std::list<Order> orders_;
    };

   private:
    void addOrder(Order new_order);
    void removeOrder(int order_id);

    std::map<int8_t, PriceLevel, std::greater<>> bids_;
    std::map<int8_t, PriceLevel> asks_;

    std::unordered_map<int8_t, std::list<Order>::iterator> order_map_;

    std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer_;
    std::shared_ptr<SPSC<MatchedOrder, 1 << 15>> matched_order_buffer_;
};

class OrderMatcher {
   public:
    OrderMatcher(
        std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer,
        std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer,
        std::shared_ptr<SPSC<MatchedOrder, 1 << 15>> matched_order_buffer);

    void start();

   private:
    std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer_;
    std::unordered_map<int8_t, InstrumentOrderMatcher> instrument_matcher_;

    // output
    std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer_;
    std::shared_ptr<SPSC<MatchedOrder, 1 << 15>> matched_order_buffer_;
};
