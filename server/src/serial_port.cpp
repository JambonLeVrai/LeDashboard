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

    // Timeouts: return immediately if no data
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout         = 50;   // ms between chars
    timeouts.ReadTotalTimeoutConstant    = 50;   // ms total
    timeouts.ReadTotalTimeoutMultiplier  = 10;   // ms per byte
    timeouts.WriteTotalTimeoutConstant   = 1000;
    timeouts.WriteTotalTimeoutMultiplier  = 10;

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

    spdlog::info("SerialPort: opened {} @ {} baud", port_name, baud_rate);
    return true;
}

void SerialPort::close() {
    if (!is_open_) return;

    stop_requested_ = true;
    is_open_ = false;

    // Cancel any pending I/O to unblock the read thread
    if (handle_ != INVALID_HANDLE_VALUE) {
        CancelIo(handle_);
    }

    if (read_thread_.joinable()) {
        read_thread_.join();
    }

    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }

    spdlog::info("SerialPort: closed {}", port_name_);
}

size_t SerialPort::write(const uint8_t *data, size_t len) {
    if (!is_open_ || handle_ == INVALID_HANDLE_VALUE) return 0;

    DWORD written = 0;
    if (!WriteFile(handle_, data, static_cast<DWORD>(len), &written, nullptr)) {
        spdlog::error("SerialPort: WriteFile failed (error {})", GetLastError());
        return 0;
    }
    return written;
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
