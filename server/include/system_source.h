#pragma once

#include "protocol.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>

// =============================================================================
// SystemSource — reads CPU/GPU/RAM/NET usage from Windows APIs
// =============================================================================

class SystemSource {
public:
    SystemSource();
    ~SystemSource();

    void start();  // background thread, samples every 5s
    void stop();

    SystemMessageBinary snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_;
    }
    bool has_data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return has_data_;
    }
    bool dirty() const { return dirty_.load(); }
    void clear_dirty() { dirty_ = false; }

private:
    void sample_loop();
    void sample_once();

    // CPU: GetSystemTimes delta
    uint64_t prev_idle_ = 0, prev_kernel_ = 0, prev_user_ = 0;
    bool first_cpu_ = true;

    // Network: GetIfTable row deltas
    uint64_t prev_net_in_ = 0, prev_net_out_ = 0;
    std::chrono::steady_clock::time_point prev_net_time_;
    bool first_net_ = true;

    mutable std::mutex mutex_;
    SystemMessageBinary last_{};
    bool has_data_ = false;
    std::atomic<bool> dirty_{false};

    std::atomic<bool> running_{false};
    std::thread thread_;

    static constexpr auto SAMPLE_INTERVAL = std::chrono::seconds(5);
};
