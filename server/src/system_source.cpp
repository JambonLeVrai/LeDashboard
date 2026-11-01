#include "system_source.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

#include <chrono>
#include <cstring>

SystemSource::SystemSource() {
    prev_net_time_ = std::chrono::steady_clock::now();
}

SystemSource::~SystemSource() {
    stop();
}

void SystemSource::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&SystemSource::sample_loop, this);
}

void SystemSource::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void SystemSource::sample_loop() {
    while (running_) {
        sample_once();
        // Sleep in 1s increments for responsive stop
        for (int i = 0; i < 5 && running_; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void SystemSource::sample_once() {
    SystemMessageBinary m{};
    m.timestamp_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

#ifdef _WIN32
    // ---- CPU via GetSystemTimes ----
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        auto to_u64 = [](const FILETIME &ft) {
            return (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
        };
        uint64_t idle_t = to_u64(idle);
        uint64_t kernel_t = to_u64(kernel);
        uint64_t user_t = to_u64(user);

        if (!first_cpu_) {
            uint64_t idle_d = idle_t - prev_idle_;
            uint64_t kernel_d = kernel_t - prev_kernel_;
            uint64_t user_d = user_t - prev_user_;
            uint64_t total = kernel_d + user_d;
            if (total > 0) {
                uint64_t busy = total - idle_d;
                m.cpu_x10 = static_cast<uint16_t>((busy * 1000) / total); // 0-1000
            }
        }
        prev_idle_ = idle_t;
        prev_kernel_ = kernel_t;
        prev_user_ = user_t;
        first_cpu_ = false;
    }

    // ---- RAM via GlobalMemoryStatusEx ----
    MEMORYSTATUSEX memInfo{};
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        uint64_t total_mb = memInfo.ullTotalPhys / (1024 * 1024);
        uint64_t used_mb = (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024);
        m.ram_total_mb = static_cast<uint16_t>(total_mb > 65535 ? 65535 : total_mb);
        m.ram_used_mb = static_cast<uint16_t>(used_mb > 65535 ? 65535 : used_mb);
    }

    // ---- Network via GetIfTable (sum of all interfaces) ----
    MIB_IFTABLE *table = nullptr;
    ULONG size = 0;
    if (GetIfTable(table, &size, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
        table = (MIB_IFTABLE *)malloc(size);
        if (table && GetIfTable(table, &size, FALSE) == NO_ERROR) {
            uint64_t in_octets = 0, out_octets = 0;
            for (DWORD i = 0; i < table->dwNumEntries; i++) {
                if (table->table[i].dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                if (table->table[i].dwOperStatus != IF_OPER_STATUS_OPERATIONAL) continue;
                in_octets += table->table[i].dwInOctets;
                out_octets += table->table[i].dwOutOctets;
            }

            auto now = std::chrono::steady_clock::now();
            if (!first_net_) {
                double elapsed_s = std::chrono::duration<double>(now - prev_net_time_).count();
                if (elapsed_s > 0.1) {
                    double down_bps = (in_octets - prev_net_in_) / elapsed_s;
                    double up_bps = (out_octets - prev_net_out_) / elapsed_s;
                    // Convert bytes/s → Mbps * 10
                    m.net_down_mbps_x10 = static_cast<uint16_t>(down_bps * 8.0 / 1000000.0 * 10.0);
                    m.net_up_mbps_x10 = static_cast<uint16_t>(up_bps * 8.0 / 1000000.0 * 10.0);
                }
            }
            prev_net_in_ = in_octets;
            prev_net_out_ = out_octets;
            prev_net_time_ = now;
            first_net_ = false;
        }
        free(table);
    }

    // ---- Disk C: via GetDiskFreeSpaceEx ----
    ULARGE_INTEGER free_avail, total_bytes, total_free;
    if (GetDiskFreeSpaceExA("C:\\", &free_avail, &total_bytes, &total_free)) {
        uint64_t used_gb = (total_bytes.QuadPart - total_free.QuadPart) / (1024ULL * 1024 * 1024);
        uint64_t total_gb = total_bytes.QuadPart / (1024ULL * 1024 * 1024);
        m.disk_used_gb = static_cast<uint16_t>(used_gb > 65535 ? 65535 : used_gb);
        m.disk_total_gb = static_cast<uint16_t>(total_gb > 65535 ? 65535 : total_gb);
    }

    // ---- Temperatures via WMI (MSAcpi_ThermalZoneTemperature in root\wmi) ----
    // Best-effort: often unavailable without admin; set 0 if not found.
    // CPU temp often available as first thermal zone; MB temp as another.
    m.cpu_temp_x10 = 0;
    m.gpu_temp_x10 = 0;
    m.mb_temp_x10 = 0;
    // (WMI query would go here; skipped for simplicity — can use OpenHardwareMonitor
    //  WMI provider or PDH \Thermal Zone Information\Temperature)
    // GPU: 0 for now (PDH \GPU Engine requires per-process; \Thermal Zone covers system)

    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_ = m;
        has_data_ = true;
        dirty_ = true;
    }

    spdlog::debug("System: CPU={:.1f}%/{:.0f}C GPU={:.1f}% RAM={}/{}MB Disk={}/{}GB NET={:.1f}/{:.1f}Mbps",
                  m.cpu_x10 / 10.0, m.cpu_temp_x10 / 10.0, m.gpu_x10 / 10.0,
                  m.ram_used_mb, m.ram_total_mb, m.disk_used_gb, m.disk_total_gb,
                  m.net_down_mbps_x10 / 10.0, m.net_up_mbps_x10 / 10.0);
#endif
}
