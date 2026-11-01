#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#endif

// =============================================================================
// SerialPort — Windows COM port with background read + write threads
// =============================================================================
//
// All writes go through a bounded queue drained by a dedicated write thread,
// so WriteFile blocking never stalls the caller.

class SerialPort {
public:
    using DataCallback = std::function<void(const uint8_t *data, size_t len)>;

    SerialPort();
    ~SerialPort();

    bool open(const std::string &port_name, uint32_t baud_rate = 115200);
    void close();
    bool is_open() const { return is_open_; }

    // Queue data for writing (non-blocking, drops oldest if queue full).
    // Returns true if queued, false if dropped.
    bool write(const uint8_t *data, size_t len);
    bool write(const std::vector<uint8_t> &data) {
        return write(data.data(), data.size());
    }

    void set_data_callback(DataCallback cb);
    const std::string &port_name() const { return port_name_; }

private:
    void read_thread_fn();
    void write_thread_fn();

#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif

    std::string port_name_;
    uint32_t baud_rate_ = 115200;
    std::atomic<bool> is_open_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread read_thread_;
    std::thread write_thread_;
    DataCallback data_callback_;
    std::mutex callback_mutex_;

    // Write queue
    std::mutex write_mutex_;
    std::condition_variable write_cv_;
    std::queue<std::vector<uint8_t>> write_queue_;
    static constexpr size_t MAX_WRITE_QUEUE = 64;
};
