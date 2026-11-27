#pragma once

#include <list>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "matching/BufferAllocator.hpp"
#include "matching/L2Data.hpp"
#include "matching/MetricsRecorder.hpp"
#include "matching/Order.hpp"
#include "matching/SPSC.hpp"
class InstrumentOrderMatcher {
   public:
    using custom_memory_pool_type = BufferMemoryPool<128, 1073741824>;

    InstrumentOrderMatcher(
        std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer,
        std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer,
        std::shared_ptr<MetricsRecorder> metrics_recorder);

    void handleOrder(Order new_order);

    class PriceLevel {
       public:
        PriceLevel(custom_memory_pool_type* pool)
            : orders_(
                  std::list<Order,
                            BufferAllocator<Order, custom_memory_pool_type>>(
                      BufferAllocator<Order, custom_memory_pool_type>(pool))) {}

        PriceLevel() = delete;

        std::vector<MatchedOrder> matchOrder(Order& new_order);

        std::list<Order>::iterator push(Order new_order);

        void erase(std::list<Order>::iterator order_pos);

        bool isEmpty();

       private:
        std::list<Order, BufferAllocator<Order, custom_memory_pool_type>>
            orders_;
    };

    InstrumentOrderMatcher() = delete;

   private:
    void addOrder(Order new_order);
    void removeOrder(int order_id);

    custom_memory_pool_type custom_memory_pool_;

    std::map<uint64_t, PriceLevel, std::greater<>,
             BufferAllocator<std::pair<const uint64_t, PriceLevel>,
                             custom_memory_pool_type>>
        bids_{BufferAllocator<std::pair<const uint64_t, PriceLevel>,
                              custom_memory_pool_type>(&custom_memory_pool_)};
    std::map<uint64_t, PriceLevel, std::less<>,
             BufferAllocator<std::pair<const uint64_t, PriceLevel>,
                             custom_memory_pool_type>>
        asks_{BufferAllocator<std::pair<const uint64_t, PriceLevel>,
                              custom_memory_pool_type>(&custom_memory_pool_)};

    std::unordered_map<uint64_t, std::list<Order>::iterator> order_map_;

    std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer_;
    std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer_;
    std::shared_ptr<MetricsRecorder> metrics_recorder_;

    InstrumentOrderMatcher(const InstrumentOrderMatcher&) = delete;
    InstrumentOrderMatcher& operator=(const InstrumentOrderMatcher&) = delete;
    InstrumentOrderMatcher(InstrumentOrderMatcher&&) = delete;
    InstrumentOrderMatcher& operator=(InstrumentOrderMatcher&&) = delete;
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
    std::unordered_map<int8_t, std::unique_ptr<InstrumentOrderMatcher>>
        instrument_matcher_;

    // output
    std::shared_ptr<DefaultSPSC<L2Data>> l2_data_buffer_;
    std::shared_ptr<DefaultSPSC<MatchedOrder>> matched_order_buffer_;
    std::shared_ptr<MetricsRecorder> metrics_recorder_;

    bool stopped = false;
};
