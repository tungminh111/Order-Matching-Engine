#pragma once

#include <iterator>
#include <list>
#include <map>
#include <memory>

#include "matching/Order.hpp"
#include "matching/OrderBuffer.hpp"
class OrderMatcher {
   public:
    OrderMatcher(std::shared_ptr<OrderBuffer<1 << 15>> order_buffer);

    void start();

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

    std::shared_ptr<OrderBuffer<1 << 15>> order_buffer_;
    std::map<int8_t, PriceLevel, std::greater<>> bids_;
    std::map<int8_t, PriceLevel> asks_;

    std::map<int8_t, std::list<Order>::iterator> order_map_;
};
