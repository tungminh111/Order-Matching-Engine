#include "matching/OrderMatcher.hpp"

#include "matching/L2Data.hpp"
#include "matching/Order.hpp"

InstrumentOrderMatcher::InstrumentOrderMatcher(
    std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer,
    std::shared_ptr<SPSC<MatchedOrder, 1 << 15>> matched_order_buffer)
    : l2_data_buffer_(l2_data_buffer),
      matched_order_buffer_(matched_order_buffer) {}

std::vector<MatchedOrder> InstrumentOrderMatcher::PriceLevel::matchOrder(
    Order& new_order) {
    std::vector<MatchedOrder> ret;
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

std::list<Order>::iterator InstrumentOrderMatcher::PriceLevel::push(
    Order new_order) {
    orders_.push_back(new_order);
    return std::prev(orders_.end());
}

void InstrumentOrderMatcher::PriceLevel::erase(
    std::list<Order>::iterator order_pos) {
    orders_.erase(order_pos);
}

bool InstrumentOrderMatcher::PriceLevel::isEmpty() {
    return orders_.size() == 0;
}

void InstrumentOrderMatcher::handleOrder(Order new_order) {
    if (new_order.action_ == OrderAction::Create) {
        addOrder(new_order);
    } else if (new_order.action_ == OrderAction::Cancel) {
        removeOrder(new_order.order_id_);
    } else {
        std::list<Order>::iterator order_pos = order_map_[new_order.order_id_];
        if (new_order.price_ == order_pos->price_ &&
            new_order.quantity_ < order_pos->quantity_) {
            order_pos->quantity_ = new_order.quantity_;
        } else {
            removeOrder(new_order.order_id_);
            addOrder(new_order);
        }
    }
}

void InstrumentOrderMatcher::addOrder(Order new_order) {
    int original_qty = new_order.quantity_;
    if (new_order.side_ == OrderSide::Buy) {
        while (new_order.quantity_ != 0 &&
               new_order.price_ >= asks_.begin()->first) {
            auto matched_orders = asks_.begin()->second.matchOrder(new_order);

            int32_t level_qty_change = 0;
            for (const MatchedOrder& matched_order : matched_orders) {
                level_qty_change -= matched_order.quantity_;
                matched_order_buffer_->write(matched_order);

                if (matched_order.match_full_) {
                    order_map_.erase(matched_order.order_id_);
                }
            }

            l2_data_buffer_->write(
                L2Data{.instrument_id_ = new_order.instrument_id_,
                       .price_level_ = asks_.begin()->first,
                       .quantity_ = level_qty_change,
                       .side_ = OrderSide::Sell});

            if (asks_.begin()->second.isEmpty()) {
                asks_.erase(asks_.begin());
            }
        }

        if (new_order.quantity_ != 0) {
            auto order_pos = bids_[new_order.price_].push(new_order);
            order_map_[new_order.order_id_] = order_pos;
        }
    } else {
        int original_qty = new_order.quantity_;
        while (new_order.quantity_ != 0 &&
               new_order.price_ <= bids_.begin()->first) {
            auto matched_orders = bids_.begin()->second.matchOrder(new_order);

            int32_t level_qty_change = 0;
            for (const MatchedOrder& matched_order : matched_orders) {
                level_qty_change -= matched_order.quantity_;
                matched_order_buffer_->write(matched_order);

                if (matched_order.match_full_) {
                    order_map_.erase(matched_order.order_id_);
                }
            }

            l2_data_buffer_->write(
                L2Data{.instrument_id_ = new_order.instrument_id_,
                       .price_level_ = bids_.begin()->first,
                       .quantity_ = level_qty_change,
                       .side_ = OrderSide::Buy});

            if (bids_.begin()->second.isEmpty()) {
                bids_.erase(bids_.begin());
            }
        }

        if (new_order.quantity_ != 0) {
            auto order_pos = asks_[new_order.price_].push(new_order);
            order_map_[new_order.order_id_] = order_pos;
        }
    }

    if (new_order.quantity_ != original_qty) {
        matched_order_buffer_->write(MatchedOrder(
            new_order.order_id_, original_qty - new_order.quantity_,
            new_order.quantity_ == 0));
    }

    if (new_order.quantity_ != 0) {
        l2_data_buffer_->write(
            L2Data{.instrument_id_ = new_order.instrument_id_,
                   .price_level_ = new_order.price_,
                   .quantity_ = new_order.quantity_,
                   .side_ = new_order.side_});
    }
}

void InstrumentOrderMatcher::removeOrder(int order_id) {
    std::list<Order>::iterator order_pos = order_map_[order_id];
    if (order_pos->side_ == OrderSide::Buy) {
        bids_[order_pos->price_].erase(order_pos);
    } else {
        asks_[order_pos->price_].erase(order_pos);
    }
    order_map_.erase(order_id);
}

void OrderMatcher::start() {
    while (true) {
        if (!order_buffer_->canRead()) continue;
        Order new_order = order_buffer_->read();
        if (instrument_matcher_.find(new_order.instrument_id_) ==
            instrument_matcher_.end()) {
            instrument_matcher_.emplace(
                new_order.instrument_id_,
                InstrumentOrderMatcher(l2_data_buffer_, matched_order_buffer_));
        }

        instrument_matcher_[new_order.instrument_id_].handleOrder(new_order);
    }
}

OrderMatcher::OrderMatcher(
    std::shared_ptr<SPSC<Order, 1 << 15>> order_buffer,
    std::shared_ptr<SPSC<L2Data, 1 << 15>> l2_data_buffer,
    std::shared_ptr<SPSC<MatchedOrder, 1 << 15>> matched_order_buffer)
    : order_buffer_(order_buffer),
      l2_data_buffer_(l2_data_buffer),
      matched_order_buffer_(matched_order_buffer) {}

