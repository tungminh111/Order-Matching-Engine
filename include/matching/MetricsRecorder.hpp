#pragma once

#include <chrono>
#include <memory>

#include "matching/SPSC.hpp"
class MetricsRecorder {
   protected:
    class MetricsTimer {
       public:
        MetricsTimer(MetricsRecorder& parent) : parent_(parent) {
            start_ = std::chrono::steady_clock::now();
        }

        ~MetricsTimer() {
            auto end = std::chrono::steady_clock::now();
            auto duration = end - start_;
            parent_.output_buffer_->tryWrite(duration.count());
        }

        MetricsTimer& operator=(const MetricsTimer& t) = delete;
        MetricsTimer& operator=(const MetricsTimer t) = delete;

       private:
        std::chrono::steady_clock::time_point start_;
        MetricsRecorder& parent_;
    };

    std::shared_ptr<DefaultSPSC<int64_t>> output_buffer_;

   public:
    MetricsTimer newTimer() { return MetricsTimer(*this); }

    MetricsRecorder(std::shared_ptr<DefaultSPSC<int64_t>> output_buffer)
        : output_buffer_(output_buffer) {}
};

