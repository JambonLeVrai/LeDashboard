#include "discover.h"
#include "protocol.h"
#include "serial_port.h"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")
#endif

#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

// =============================================================================
// List all COM ports via SetupAPI (PnP Ports class)
// =============================================================================

std::vector<std::string> list_com_ports() {
    std::vector<std::string> ports;
#ifdef _WIN32
    // Read HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM — lists only present COM ports
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char valueName[256];
        BYTE data[256];
        DWORD index = 0;
        while (true) {
            DWORD nameLen = sizeof(valueName);
            DWORD dataLen = sizeof(data);
            DWORD type = 0;
            LONG result = RegEnumValueA(hKey, index++, valueName, &nameLen, nullptr,
                                        &type, data, &dataLen);
            if (result != ERROR_SUCCESS) break;
            if (type == REG_SZ) {
                std::string portName(reinterpret_cast<char *>(data));
                // Only add if it looks like COMx
                if (portName.rfind("COM", 0) == 0) {
                    ports.push_back(portName);
                }
            }
        }
        RegCloseKey(hKey);
    }
    // Fallback: if registry read failed, try COM1..COM20
    if (ports.empty()) {
        for (int i = 1; i <= 20; i++) {
            ports.push_back("COM" + std::to_string(i));
        }
    }
#endif
    return ports;
}

// =============================================================================
// Discover: open each port, send DISCOVER, wait for DISCOVER_ACK
// =============================================================================

std::string discover_peripheral(uint32_t baud_rate, int timeout_per_port_ms) {
    auto ports = list_com_ports();
    spdlog::info("Discovery: scanning {} COM ports...", ports.size());

    // Build the probe frame once
    auto probe = encode_frame(static_cast<MsgType>(MSG_DISCOVER),
                              reinterpret_cast<const uint8_t *>(DISCOVER_MAGIC),
                              strlen(DISCOVER_MAGIC));

    for (const auto &port : ports) {
        spdlog::debug("Discovery: probing {}...", port);

        SerialPort sp;
        if (!sp.open(port, baud_rate)) {
            continue; // can't open (in use, doesn't exist, etc.)
        }

        std::atomic<bool> got_response{false};
        std::string response_payload;
        std::mutex resp_mutex;

        FrameDecoder decoder(
            [&](const DecodedFrame &f) {
                if (f.type == static_cast<MsgType>(MSG_DISCOVER_ACK)) {
                    std::lock_guard<std::mutex> lock(resp_mutex);
                    response_payload.assign(f.payload.begin(), f.payload.end());
                    got_response = true;
                }
            },
            nullptr
        );

        sp.set_data_callback([&](const uint8_t *data, size_t len) {
            decoder.feed(data, len);
        });

        // Send probe
        sp.write(probe);

        // Wait for response
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_per_port_ms);
        while (std::chrono::steady_clock::now() < deadline && !got_response) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        sp.close();

        if (got_response) {
            std::string resp;
            {
                std::lock_guard<std::mutex> lock(resp_mutex);
                resp = response_payload;
            }
            if (resp.find("LEDASHBOARD_ESP32") != std::string::npos) {
                spdlog::info("Discovery: found LeDashboard peripheral on {} ({})", port, resp);
                return port;
            }
        }
    }

    spdlog::warn("Discovery: no LeDashboard peripheral found on any COM port");
    return "";
}
