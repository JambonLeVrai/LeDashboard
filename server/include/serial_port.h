#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

// =============================================================================
// SerialPort — Windows COM port with background read thread
// =============================================================================

class SerialPort {
public:
    using DataCallback = std::function<void(const uint8_t *data, size_t len)>;

    SerialPort();
    ~SerialPort();

    // Open the named port (e.g. "COM5") at the given baud rate.
    bool open(const std::string &port_name, uint32_t baud_rate = 115200);
    void close();
    bool is_open() const { return is_open_; }

    // Write raw bytes. Returns number of bytes written.
    size_t write(const uint8_t *data, size_t len);
    size_t write(const std::vector<uint8_t> &data) {
        return write(data.data(), data.size());
    }

    // Set the callback invoked (on the read thread) when data arrives.
    void set_data_callback(DataCallback cb);

    const std::string &port_name() const { return port_name_; }

private:
    void read_thread_fn();

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
    DataCallback data_callback_;
    std::mutex callback_mutex_;
};
