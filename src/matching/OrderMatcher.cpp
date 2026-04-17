#include "matching/OrderMatcher.hpp"

#include "matching/Order.hpp"

std::vector<OrderQuantity> OrderMatcher::PriceLevel::matchOrder(
    Order& new_order) {
    std::vector<OrderQuantity> ret;
    while (!orders_.empty()) {
        int8_t matchedQuantity =
            std::min(new_order.quantity_, orders_.front().quantity_);
        orders_.front().quantity_ -= matchedQuantity;
        new_order.quantity_ -= matchedQuantity;

        ret.emplace_back(orders_.front().order_id_, matchedQuantity,
                         orders_.front().quantity_ == 0);
        if (orders_.front().quantity_ == 0) {
            orders_.pop_front();
        }

        if (new_order.quantity_ == 0) {
            break;
        }
    }

    return ret;
}

std::list<Order>::iterator OrderMatcher::PriceLevel::push(Order new_order) {
    orders_.push_back(new_order);
    return std::prev(orders_.end());
}

void OrderMatcher::PriceLevel::erase(std::list<Order>::iterator order_pos) {
    orders_.erase(order_pos);
}

bool OrderMatcher::PriceLevel::isEmpty() { return orders_.size() == 0; }

void OrderMatcher::start() {
    while (true) {
        if (!order_buffer_->canRead()) continue;

        Order new_order = order_buffer_->read();
        if (new_order.action_ == OrderAction::Create) {
            addOrder(new_order);
        } else if (new_order.action_ == OrderAction::Cancel) {
            removeOrder(new_order.order_id_);
        } else {
            std::list<Order>::iterator order_pos =
                order_map_[new_order.order_id_];
            if (new_order.price_ == order_pos->price_ &&
                new_order.quantity_ < order_pos->quantity_) {
                order_pos->quantity_ = new_order.quantity_;
            } else {
                removeOrder(new_order.order_id_);
                addOrder(new_order);
            }
        }
    }
}

void OrderMatcher::addOrder(Order new_order) {
    if (new_order.side_ == OrderSide::Buy) {
        int original_qty = new_order.quantity_;
        while (new_order.quantity_ != 0 &&
               new_order.price_ >= asks_.begin()->first) {
            auto matchedOrders = asks_.begin()->second.matchOrder(new_order);

            int32_t level_price_change = 0;
            for (const OrderQuantity& order_quantity : matchedOrders) {
                level_price_change += order_quantity.quantity_;
                // notify matched trade
            }

            // send L2 data

            if (asks_.begin()->second.isEmpty()) {
                asks_.erase(asks_.begin());
            }
        }

        if (new_order.quantity_ != original_qty) {
            // notify matched order
        }

        if (new_order.quantity_ != 0) {
            auto order_pos = bids_[new_order.price_].push(new_order);
            order_map_[new_order.order_id_] = order_pos;
            // notify level price change
        }
    } else {
        int original_qty = new_order.quantity_;
        while (new_order.quantity_ != 0 &&
               new_order.price_ <= bids_.begin()->first) {
            auto matchedOrders = bids_.begin()->second.matchOrder(new_order);

            int32_t level_price_change = 0;
            for (const OrderQuantity& order_quantity : matchedOrders) {
                level_price_change += order_quantity.quantity_;
                // notify matched trade
            }

            // send L2 data

            if (bids_.begin()->second.isEmpty()) {
                bids_.erase(bids_.begin());
            }
        }

        if (new_order.quantity_ != original_qty) {
            // notify matched order
        }

        if (new_order.quantity_ != 0) {
            auto order_pos = asks_[new_order.price_].push(new_order);
            order_map_[new_order.order_id_] = order_pos;
            // notify level price change
        }
    }
}

void OrderMatcher::removeOrder(int order_id) {
    std::list<Order>::iterator order_pos = order_map_[order_id];
    if (order_pos->side_ == OrderSide::Buy) {
        bids_[order_pos->price_].erase(order_pos);
    } else {
        asks_[order_pos->price_].erase(order_pos);
    }
    order_map_.erase(order_id);
}
