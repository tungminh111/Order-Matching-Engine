#pragma once

#include <list>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "matching/L2Data.hpp"
#include "matching/MetricsRecorder.hpp"
#include "matching/Order.hpp"
#include "matching/SPSC.hpp"
class InstrumentOrderMatcher {
   public:
    InstrumentOrderMatcher(
        std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer,
        std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer,
        std::shared_ptr<MetricsRecorder> metrics_recorder);

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

    InstrumentOrderMatcher() = delete;

   private:
    void addOrder(Order new_order);
    void removeOrder(int order_id);

    std::map<uint64_t, PriceLevel, std::greater<>> bids_;
    std::map<uint64_t, PriceLevel> asks_;

    std::unordered_map<uint64_t, std::list<Order>::iterator> order_map_;

    std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer_;
    std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer_;
    std::shared_ptr<MetricsRecorder> metrics_recorder_;
};

class OrderMatcher {
   public:
    OrderMatcher(
        std::shared_ptr<DefaultSPSC<Order>> order_buffer,
        std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer,
        std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer,
        std::shared_ptr<MetricsRecorder> metrics_recorder);

    void start();
    void stop();

   private:
    std::shared_ptr<DefaultSPSC<Order>> order_buffer_;
    std::unordered_map<int8_t, InstrumentOrderMatcher> instrument_matcher_;

    // output
    std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer_;
    std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer_;
    std::shared_ptr<MetricsRecorder> metrics_recorder_;

    bool stopped = false;
};
