#include "matching/OrderMatcher.hpp"

#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "matching/L2Data.hpp"
#include "matching/Order.hpp"

InstrumentOrderMatcher::InstrumentOrderMatcher(
    std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer,
    std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer,
    std::shared_ptr<MetricsRecorder> metrics_recorder)
    : l2_data_buffer_(l2_data_buffer),
      matched_order_buffer_(matched_order_buffer),
      metrics_recorder_(metrics_recorder) {
    order_map_.reserve(1 << 21);
}

int64_t InstrumentOrderMatcher::PriceLevel::matchOrder(Order& new_order) {
    uint64_t total_matched_qty = 0;

    while (!orders_.empty()) {
        uint64_t matched_quantity =
            std::min(new_order.quantity_, orders_.front().quantity_);
        orders_.front().quantity_ -= matched_quantity;
        new_order.quantity_ -= matched_quantity;

        matched_order_callback_(MatchedOrder{
            .order_id_ = orders_.front().order_id_,
            .quantity_ = matched_quantity,
            .match_full_ = orders_.front().quantity_ == 0,
        });

        matched_order_callback_(MatchedOrder{
            .order_id_ = new_order.order_id_,
            .quantity_ = matched_quantity,
            .match_full_ = new_order.quantity_ == 0,
        });

        total_matched_qty += matched_quantity;

        if (orders_.front().quantity_ == 0) {
            orders_.pop_front();
        }

        if (new_order.quantity_ == 0) {
            break;
        }
    }
    return total_matched_qty;
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
    auto timer = metrics_recorder_->newTimer();

    if (new_order.action_ == OrderAction::Create) {
        addOrder(new_order);
    } else if (new_order.action_ == OrderAction::Cancel) {
        removeOrder(new_order.order_id_);
    } else {
        if (!order_map_.contains(new_order.order_id_))
            throw std::runtime_error("modify non exist order is not allowed");
        std::list<Order>::iterator order_pos =
            order_map_.at(new_order.order_id_);
        if (new_order.price_ == 0) new_order.price_ = order_pos->price_;
        if (new_order.price_ == order_pos->price_ &&
            new_order.quantity_ < order_pos->quantity_) {
            l2_data_buffer_->write(
                L2Data{.instrument_id_ = new_order.instrument_id_,
                       .price_level_ = order_pos->price_,
                       .quantity_ = static_cast<int64_t>(new_order.quantity_) -
                                    static_cast<int64_t>(order_pos->quantity_),
                       .side_ = order_pos->side_});

            order_pos->quantity_ = new_order.quantity_;
        } else {
            removeOrder(new_order.order_id_);
            addOrder(new_order);
        }
    }
}

void InstrumentOrderMatcher::addOrder(Order new_order) {
    if (order_map_.contains(new_order.order_id_))
        throw std::runtime_error("adding duplicate order is not allowed");
    if (new_order.type_ == OrderType::Market) {
        if (new_order.side_ == OrderSide::Buy) {
            new_order.price_ =
                std::numeric_limits<typeof(new_order.price_)>::max();
        } else {
            new_order.price_ =
                std::numeric_limits<typeof(new_order.price_)>::min();
        }
    }
    if (new_order.side_ == OrderSide::Buy) {
        while (new_order.quantity_ != 0 && !asks_.empty() &&
               new_order.price_ >= asks_.begin()->first) {
            auto level_qty_change = asks_.begin()->second.matchOrder(new_order);

            l2_data_buffer_->write(
                L2Data{.instrument_id_ = new_order.instrument_id_,
                       .price_level_ = asks_.begin()->first,
                       .quantity_ = -level_qty_change,
                       .side_ = OrderSide::Sell});

            if (asks_.begin()->second.isEmpty()) {
                asks_.erase(asks_.begin());
            }
        }

        if (new_order.quantity_ != 0 && new_order.type_ == OrderType::Limit) {
            if (!bids_.contains(new_order.price_)) {
                bids_.emplace(new_order.price_, newPriceLevel());
            }
            auto order_pos = bids_.at(new_order.price_).push(new_order);
            order_map_.emplace(new_order.order_id_, order_pos);
        }
    } else {
        while (new_order.quantity_ != 0 && !bids_.empty() &&
               new_order.price_ <= bids_.begin()->first) {
            auto level_qty_change = bids_.begin()->second.matchOrder(new_order);

            l2_data_buffer_->write(
                L2Data{.instrument_id_ = new_order.instrument_id_,
                       .price_level_ = bids_.begin()->first,
                       .quantity_ = -level_qty_change,
                       .side_ = OrderSide::Buy});

            if (bids_.begin()->second.isEmpty()) {
                bids_.erase(bids_.begin());
            }
        }

        if (new_order.quantity_ != 0 && new_order.type_ == OrderType::Limit) {
            if (!asks_.contains(new_order.price_)) {
                asks_.emplace(new_order.price_, newPriceLevel());
            }
            auto order_pos = asks_.at(new_order.price_).push(new_order);
            order_map_.emplace(new_order.order_id_, order_pos);
        }
    }

    if (new_order.quantity_ != 0 && new_order.type_ == OrderType::Limit) {
        l2_data_buffer_->write(
            L2Data{.instrument_id_ = new_order.instrument_id_,
                   .price_level_ = new_order.price_,
                   .quantity_ = static_cast<int64_t>(new_order.quantity_),
                   .side_ = new_order.side_});
    }
}

void InstrumentOrderMatcher::handleMatchedOrder(void* ctx,
                                                const MatchedOrder& order) {
    auto this_ptr = static_cast<InstrumentOrderMatcher*>(ctx);
    this_ptr->matched_order_buffer_->write(order);
    if (order.match_full_) this_ptr->order_map_.erase(order.order_id_);
}

InstrumentOrderMatcher::PriceLevel InstrumentOrderMatcher::newPriceLevel() {
    return InstrumentOrderMatcher::PriceLevel(
        &custom_memory_pool_,
        MatchedOrderCallback(this, InstrumentOrderMatcher::handleMatchedOrder));
}

void InstrumentOrderMatcher::removeOrder(int order_id) {
    auto it = order_map_.find(order_id);
    if (it == order_map_.end()) {
        throw std::runtime_error("fail to remove non exist order");
    }

    std::list<Order>::iterator order_pos = it->second;
    l2_data_buffer_->write(
        L2Data{.instrument_id_ = order_pos->instrument_id_,
               .price_level_ = order_pos->price_,
               .quantity_ = -static_cast<int64_t>(order_pos->quantity_),
               .side_ = order_pos->side_});

    if (order_pos->side_ == OrderSide::Buy) {
        auto it = bids_.find(order_pos->price_);
        it->second.erase(order_pos);
        if (it->second.isEmpty()) {
            bids_.erase(it);
        }
    } else {
        auto it = asks_.find(order_pos->price_);
        it->second.erase(order_pos);
        if (it->second.isEmpty()) {
            asks_.erase(it);
        }
    }
    order_map_.erase(order_id);
}

void OrderMatcher::start() {
    while (!stopped || order_buffer_->canRead()) {
        if (!order_buffer_->canRead()) continue;
        Order new_order = order_buffer_->read();
        if (instrument_matcher_.find(new_order.instrument_id_) ==
            instrument_matcher_.end()) {
            instrument_matcher_.emplace(
                new_order.instrument_id_,
                std::make_unique<InstrumentOrderMatcher>(
                    l2_data_buffer_, matched_order_buffer_, metrics_recorder_));
        }

        instrument_matcher_.at(new_order.instrument_id_)
            ->handleOrder(new_order);
    }
}

OrderMatcher::OrderMatcher(
    std::shared_ptr<DefaultSPSC<Order>> order_buffer,
    std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer,
    std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer,
    std::shared_ptr<MetricsRecorder> metrics_recorder)

    : order_buffer_(order_buffer),
      l2_data_buffer_(l2_data_buffer),
      matched_order_buffer_(matched_order_buffer),
      metrics_recorder_(metrics_recorder) {}

void OrderMatcher::stop() { stopped = true; }
