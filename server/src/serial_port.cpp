#include "serial_port.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32

SerialPort::SerialPort() = default;

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string &port_name, uint32_t baud_rate) {
    if (is_open_) {
        spdlog::warn("SerialPort: already open on {}", port_name_);
        return true;
    }

    port_name_ = port_name;
    baud_rate_ = baud_rate;

    // Windows: open as \\.\COMx for ports > 9
    std::string device = port_name;
    if (port_name.rfind("COM", 0) == 0) {
        int num = std::stoi(port_name.substr(3));
        if (num > 9) {
            device = "\\\\.\\" + port_name;
        }
    }

    handle_ = CreateFileA(device.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, nullptr,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);

    if (handle_ == INVALID_HANDLE_VALUE) {
        spdlog::error("SerialPort: failed to open {} (error {})", device, GetLastError());
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) {
        spdlog::error("SerialPort: GetCommState failed (error {})", GetLastError());
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    dcb.BaudRate = baud_rate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(handle_, &dcb)) {
        spdlog::error("SerialPort: SetCommState failed (error {})", GetLastError());
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    // Timeouts: non-blocking writes (drop if buffer full — next frame comes in 1s)
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout         = 10;
    timeouts.ReadTotalTimeoutConstant    = 10;
    timeouts.ReadTotalTimeoutMultiplier  = 1;
    timeouts.WriteTotalTimeoutConstant   = 0;   // non-blocking writes
    timeouts.WriteTotalTimeoutMultiplier  = 0;

    if (!SetCommTimeouts(handle_, &timeouts)) {
        spdlog::error("SerialPort: SetCommTimeouts failed (error {})", GetLastError());
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    // Set buffer sizes
    SetupComm(handle_, 8192, 8192);

    is_open_ = true;
    stop_requested_ = false;
    read_thread_ = std::thread(&SerialPort::read_thread_fn, this);
    write_thread_ = std::thread(&SerialPort::write_thread_fn, this);

    spdlog::info("SerialPort: opened {} @ {} baud", port_name, baud_rate);
    return true;
}

void SerialPort::close() {
    if (!is_open_) return;

    stop_requested_ = true;
    is_open_ = false;

    write_cv_.notify_all();

    if (handle_ != INVALID_HANDLE_VALUE) {
        CancelIo(handle_);
    }

    if (read_thread_.joinable()) read_thread_.join();
    if (write_thread_.joinable()) write_thread_.join();

    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }

    spdlog::info("SerialPort: closed {}", port_name_);
}

bool SerialPort::write(const uint8_t *data, size_t len) {
    if (!is_open_) return false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (write_queue_.size() >= MAX_WRITE_QUEUE) {
            write_queue_.pop(); // drop oldest
        }
        write_queue_.emplace(data, data + len);
    }
    write_cv_.notify_one();
    return true;
}

void SerialPort::write_thread_fn() {
    while (!stop_requested_ || !is_open_) {
        std::vector<uint8_t> frame;
        {
            std::unique_lock<std::mutex> lock(write_mutex_);
            write_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return stop_requested_ || !write_queue_.empty();
            });
            if (stop_requested_) break;
            if (write_queue_.empty()) continue;
            frame = std::move(write_queue_.front());
            write_queue_.pop();
        }

        if (handle_ != INVALID_HANDLE_VALUE && !frame.empty()) {
            DWORD written = 0;
            // Blocking write is OK here — only this thread does WriteFile
            WriteFile(handle_, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr);
        }
    }
}

void SerialPort::set_data_callback(DataCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    data_callback_ = std::move(cb);
}

void SerialPort::read_thread_fn() {
    uint8_t buf[1024];
    while (!stop_requested_ && is_open_) {
        DWORD bytes_read = 0;
        BOOL ok = ReadFile(handle_, buf, sizeof(buf), &bytes_read, nullptr);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                // Overlapped not used; should not happen with our settings
                continue;
            }
            if (stop_requested_) break;
            spdlog::warn("SerialPort: ReadFile error {} on {}", err, port_name_);
            break;
        }
        if (bytes_read > 0) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (data_callback_) {
                data_callback_(buf, bytes_read);
            }
        }
    }
}

#else
// POSIX stub (not used on Windows dev machine)
#error "POSIX serial not implemented in this build"
#endif
