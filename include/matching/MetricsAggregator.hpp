#pragma once

#include <memory>
#include <vector>
#include "matching/SPSC.hpp"
class MetricsAggregator{
    public:
        MetricsAggregator(std::shared_ptr<DefaultSPSC<int64_t>> input_buffer): input_buffer_(input_buffer) {}

        void start() {
        }

    private:
    std::shared_ptr<DefaultSPSC<int64_t>> input_buffer_;
};
