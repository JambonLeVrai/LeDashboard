#include "system_source.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")
#endif

#include <chrono>
#include <cstring>
#include <cmath>

#ifdef _WIN32
// =============================================================================
// CPU temp via WMI performance counter class (root\cimv2):
// Win32_PerfRawData_Counters_ThermalZoneInformation — Temperature in Kelvin
// =============================================================================
static double read_cpu_temp_wmi() {
    HRESULT hr;
    IWbemLocator *loc = nullptr;
    IWbemServices *svc = nullptr;
    IEnumWbemClassObject *enu = nullptr;
    double max_temp_c = 0.0;

    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (LPVOID *)&loc);
    if (FAILED(hr) || !loc) return 0.0;

    // Try root\cimv2 performance thermal zone first (most reliable)
    hr = loc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, 0, 0, &svc);
    if (FAILED(hr) || !svc) { loc->Release(); return 0.0; }

    CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    hr = svc->ExecQuery(_bstr_t(L"WQL"),
        _bstr_t(L"SELECT Temperature FROM Win32_PerfRawData_Counters_ThermalZoneInformation"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enu);
    if (SUCCEEDED(hr) && enu) {
        while (enu) {
            IWbemClassObject *obj = nullptr;
            ULONG ret = 0;
            if (enu->Next(5000, 1, &obj, &ret) != S_OK || ret == 0) break;
            VARIANT vt;
            VariantInit(&vt);
            if (SUCCEEDED(obj->Get(L"Temperature", 0, &vt, nullptr, nullptr))) {
                double kelvin = 0;
                if (vt.vt == VT_I4 || vt.vt == VT_UI4) kelvin = (double)vt.lVal;
                else if (vt.vt == VT_BSTR) kelvin = _wtof(vt.bstrVal);
                if (kelvin > 0) {
                    double celsius = kelvin - 273.15;
                    if (celsius > max_temp_c) max_temp_c = celsius;
                }
            }
            VariantClear(&vt);
            obj->Release();
        }
        enu->Release();
    }
    svc->Release();

    // Fallback: root\wmi MSAcpi_ThermalZoneTemperature (Kelvin*10)
    if (max_temp_c <= 0.0) {
        IWbemServices *svc2 = nullptr;
        hr = loc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, 0, 0, 0, 0, &svc2);
        if (SUCCEEDED(hr) && svc2) {
            CoSetProxyBlanket(svc2, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
            IEnumWbemClassObject *enu2 = nullptr;
            hr = svc2->ExecQuery(_bstr_t(L"WQL"),
                _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enu2);
            if (SUCCEEDED(hr) && enu2) {
                while (enu2) {
                    IWbemClassObject *obj = nullptr;
                    ULONG ret = 0;
                    if (enu2->Next(5000, 1, &obj, &ret) != S_OK || ret == 0) break;
                    VARIANT vt;
                    VariantInit(&vt);
                    if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &vt, nullptr, nullptr))) {
                        if (vt.vt == VT_I4 || vt.vt == VT_UI4) {
                            double celsius = vt.lVal / 10.0 - 273.15;
                            if (celsius > max_temp_c) max_temp_c = celsius;
                        }
                    }
                    VariantClear(&vt);
                    obj->Release();
                }
                enu2->Release();
            }
            svc2->Release();
        }
    }

    loc->Release();
    return max_temp_c;
}

// =============================================================================
// GPU temp via NVML (nvml.dll, dynamic load — NVIDIA only)
// =============================================================================
typedef int (*nvmlInit_v2_t)();
typedef int (*nvmlDeviceGetHandleByIndex_t)(unsigned int, void **);
typedef int (*nvmlDeviceGetTemperature_t)(void *, unsigned int, unsigned int *);
typedef const char *(*nvmlErrorString_t)(int);
#define NVML_TEMP_GPU 0

static double read_gpu_temp_nvml() {
    HMODULE nvml = LoadLibraryA("nvml.dll");
    if (!nvml) return 0.0;

    auto nvmlInit = (nvmlInit_v2_t)GetProcAddress(nvml, "nvmlInit_v2");
    auto getHandle = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(nvml, "nvmlDeviceGetHandleByIndex_v2");
    auto getTemp = (nvmlDeviceGetTemperature_t)GetProcAddress(nvml, "nvmlDeviceGetTemperature");
    if (!nvmlInit || !getHandle || !getTemp) { FreeLibrary(nvml); return 0.0; }

    if (nvmlInit() != 0) { FreeLibrary(nvml); return 0.0; }
    void *handle = nullptr;
    if (getHandle(0, &handle) != 0 || !handle) { FreeLibrary(nvml); return 0.0; }
    unsigned int temp = 0;
    if (getTemp(handle, NVML_TEMP_GPU, &temp) != 0) { FreeLibrary(nvml); return 0.0; }
    FreeLibrary(nvml);
    return (double)temp;
}
#endif // _WIN32

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

    // ---- Disk I/O via PDH: \PhysicalDisk(_Total)\% Disk Time ----
    m.disk_io_x10 = 0;
    {
        PDH_HQUERY query = nullptr;
        PDH_HCOUNTER counter = nullptr;
        if (PdhOpenQueryA(nullptr, 0, &query) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounterA(query, "\\PhysicalDisk(_Total)\\% Disk Time", 0, &counter) == ERROR_SUCCESS) {
                if (PdhCollectQueryData(query) == ERROR_SUCCESS) {
                    PdhCollectQueryData(query); // need 2 samples for rate counters
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    PdhCollectQueryData(query);
                    PDH_FMT_COUNTERVALUE val;
                    if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
                        double pct = val.doubleValue;
                        if (pct < 0) pct = 0;
                        if (pct > 100) pct = 100;
                        m.disk_io_x10 = static_cast<uint16_t>(pct * 10.0);
                    }
                }
            }
            PdhCloseQuery(query);
        }
    }

    // ---- Temperatures ----
    m.cpu_temp_x10 = static_cast<uint16_t>(read_cpu_temp_wmi() * 10.0);
    m.gpu_temp_x10 = static_cast<uint16_t>(read_gpu_temp_nvml() * 10.0);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_ = m;
        has_data_ = true;
        dirty_ = true;
    }

    spdlog::debug("System: CPU={:.1f}%/{:.0f}C GPU={:.1f}%/{:.0f}C RAM={}/{}MB DiskIO={:.0f}% NET={:.1f}/{:.1f}Mbps",
                  m.cpu_x10 / 10.0, m.cpu_temp_x10 / 10.0, m.gpu_x10 / 10.0, m.gpu_temp_x10 / 10.0,
                  m.ram_used_mb, m.ram_total_mb, m.disk_io_x10 / 10.0,
                  m.net_down_mbps_x10 / 10.0, m.net_up_mbps_x10 / 10.0);
#endif
}
