#pragma once

#include <list>
#include <map>
#include <memory>
#include <unordered_map>

#include "matching/Order.hpp"
#include "matching/OrderBuffer.hpp"
class InstrumentOrderMatcher {
   public:
    InstrumentOrderMatcher();

    void handleOrder(Order new_order);

    class PriceLevel {
       public:
        std::vector<OrderQuantity> matchOrder(Order& new_order);

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
};

class OrderMatcher {
   public:
    OrderMatcher(std::shared_ptr<OrderBuffer<1 << 15>> order_buffer);

    void start();

   private:
    std::shared_ptr<OrderBuffer<1 << 15>> order_buffer_;
    std::unordered_map<int8_t, InstrumentOrderMatcher> instrument_matcher_;
};
