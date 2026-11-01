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
// CPU temp via WMI (root\cimv2 perf thermal zones, fallback root\wmi ACPI)
// =============================================================================
static double read_cpu_temp_wmi() {
    HRESULT hr;
    IWbemLocator *loc = nullptr;
    double max_temp_c = 0.0;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (LPVOID *)&loc);
    if (FAILED(hr) || !loc) return 0.0;

    IWbemServices *svc = nullptr;
    hr = loc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, 0, 0, &svc);
    if (SUCCEEDED(hr) && svc) {
        CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        IEnumWbemClassObject *enu = nullptr;
        hr = svc->ExecQuery(_bstr_t(L"WQL"),
            _bstr_t(L"SELECT Temperature FROM Win32_PerfRawData_Counters_ThermalZoneInformation"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enu);
        if (SUCCEEDED(hr) && enu) {
            while (enu) {
                IWbemClassObject *obj = nullptr;
                ULONG ret = 0;
                if (enu->Next(2000, 1, &obj, &ret) != S_OK || ret == 0) break;
                VARIANT vt; VariantInit(&vt);
                if (SUCCEEDED(obj->Get(L"Temperature", 0, &vt, nullptr, nullptr))) {
                    double kelvin = 0;
                    if (vt.vt == VT_I4 || vt.vt == VT_UI4) kelvin = (double)vt.lVal;
                    else if (vt.vt == VT_BSTR) kelvin = _wtof(vt.bstrVal);
                    if (kelvin > 0) {
                        double c = kelvin - 273.15;
                        if (c > max_temp_c) max_temp_c = c;
                    }
                }
                VariantClear(&vt); obj->Release();
            }
            enu->Release();
        }
        svc->Release();
    }
    loc->Release();
    return max_temp_c;
}

// =============================================================================
// GPU via NVML (NVIDIA dedicated GPU) — usage % + temp
// =============================================================================
typedef int (*nvmlInit_v2_t)();
typedef int (*nvmlDeviceGetHandleByIndex_t)(unsigned int, void **);
typedef int (*nvmlDeviceGetTemperature_t)(void *, unsigned int, unsigned int *);
typedef int (*nvmlDeviceGetUtilizationRates_t)(void *, void *);
struct nvmlUtilization_t { unsigned int gpu; unsigned int memory; };
#define NVML_TEMP_GPU 0

struct NvmlState {
    HMODULE lib = nullptr;
    void *handle = nullptr;
    nvmlDeviceGetTemperature_t getTemp = nullptr;
    nvmlDeviceGetUtilizationRates_t getUtil = nullptr;
    bool ok() const { return lib && handle && getTemp && getUtil; }
};

static NvmlState init_nvml() {
    NvmlState s;
    s.lib = LoadLibraryA("nvml.dll");
    if (!s.lib) return s;
    auto init = (nvmlInit_v2_t)GetProcAddress(s.lib, "nvmlInit_v2");
    auto getHandle = (nvmlDeviceGetHandleByIndex_t)GetProcAddress(s.lib, "nvmlDeviceGetHandleByIndex_v2");
    s.getTemp = (nvmlDeviceGetTemperature_t)GetProcAddress(s.lib, "nvmlDeviceGetTemperature");
    s.getUtil = (nvmlDeviceGetUtilizationRates_t)GetProcAddress(s.lib, "nvmlDeviceGetUtilizationRates");
    if (!init || !getHandle || !s.getTemp || !s.getUtil) { FreeLibrary(s.lib); s = NvmlState{}; return s; }
    if (init() != 0) { FreeLibrary(s.lib); s = NvmlState{}; return s; }
    if (getHandle(0, &s.handle) != 0 || !s.handle) { FreeLibrary(s.lib); s = NvmlState{}; return s; }
    return s;
}

static void read_gpu_nvml(const NvmlState &nv, double &usage_pct, double &temp_c) {
    usage_pct = 0; temp_c = 0;
    if (!nv.ok()) return;
    unsigned int t = 0;
    if (nv.getTemp(nv.handle, NVML_TEMP_GPU, &t) == 0) temp_c = (double)t;
    nvmlUtilization_t u{};
    if (nv.getUtil(nv.handle, &u) == 0) usage_pct = (double)u.gpu;
}
#endif // _WIN32

SystemSource::SystemSource() {
    prev_net_time_ = std::chrono::steady_clock::now();
}

SystemSource::~SystemSource() { stop(); }

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
    auto last_emit = std::chrono::steady_clock::now() - EMIT_INTERVAL;
    while (running_) {
        sample_once();
        auto now = std::chrono::steady_clock::now();
        if (now - last_emit >= EMIT_INTERVAL) {
            last_emit = now;
            emit();
        }
        // Sleep in 100ms increments for responsive stop, target 500ms poll
        for (int i = 0; i < 5 && running_; i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SystemSource::sample_once() {
    float cpu_pct = 0, disk_pct = 0, net_down = 0, net_up = 0;
    double gpu_pct = 0;

#ifdef _WIN32
    // ---- CPU via GetSystemTimes ----
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        auto to64 = [](const FILETIME &ft) { return (uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime; };
        uint64_t i_t = to64(idle), k_t = to64(kernel), u_t = to64(user);
        if (!first_cpu_) {
            uint64_t total = (k_t - prev_kernel_) + (u_t - prev_user_);
            if (total > 0) cpu_pct = (float)((total - (i_t - prev_idle_)) * 100.0 / total);
        }
        prev_idle_ = i_t; prev_kernel_ = k_t; prev_user_ = u_t;
        first_cpu_ = false;
    }

    // ---- GPU via NVML (dedicated NVIDIA) ----
    static NvmlState nv = init_nvml();
    double gpu_temp = 0;
    read_gpu_nvml(nv, gpu_pct, gpu_temp);

    // ---- Disk I/O via PDH ----
    {
        PDH_HQUERY q = nullptr; PDH_HCOUNTER c = nullptr;
        if (PdhOpenQueryA(nullptr, 0, &q) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounterA(q, "\\PhysicalDisk(_Total)\\% Disk Time", 0, &c) == ERROR_SUCCESS) {
                PdhCollectQueryData(q);
                // PDH rate counters need 2 samples; use PERF_AVERAGE_TIMER... simplest: collect twice with tiny sleep
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                PdhCollectQueryData(q);
                PDH_FMT_COUNTERVALUE v;
                if (PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &v) == ERROR_SUCCESS) {
                    disk_pct = (float)v.doubleValue;
                    if (disk_pct < 0) disk_pct = 0;
                    if (disk_pct > 100) disk_pct = 100;
                }
            }
            PdhCloseQuery(q);
        }
    }

    // ---- Network via GetIfTable ----
    {
        MIB_IFTABLE *table = nullptr;
        ULONG size = 0;
        if (GetIfTable(table, &size, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
            table = (MIB_IFTABLE *)malloc(size);
            if (table && GetIfTable(table, &size, FALSE) == NO_ERROR) {
                uint64_t in_o = 0, out_o = 0;
                for (DWORD i = 0; i < table->dwNumEntries; i++) {
                    if (table->table[i].dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                    if (table->table[i].dwOperStatus != IF_OPER_STATUS_OPERATIONAL) continue;
                    in_o += table->table[i].dwInOctets;
                    out_o += table->table[i].dwOutOctets;
                }
                auto now = std::chrono::steady_clock::now();
                if (!first_net_) {
                    double es = std::chrono::duration<double>(now - prev_net_time_).count();
                    if (es > 0.05) {
                        net_down = (float)((in_o - prev_net_in_) / es * 8.0 / 1e6); // Mbps
                        net_up   = (float)((out_o - prev_net_out_) / es * 8.0 / 1e6);
                    }
                }
                prev_net_in_ = in_o; prev_net_out_ = out_o;
                prev_net_time_ = now; first_net_ = false;
            }
            free(table);
        }
    }

    // Push into ring buffers
    ring_cpu_[ring_idx_] = cpu_pct;
    ring_gpu_[ring_idx_] = (float)gpu_pct;
    ring_disk_[ring_idx_] = disk_pct;
    ring_net_down_[ring_idx_] = net_down;
    ring_net_up_[ring_idx_] = net_up;
    ring_idx_ = (ring_idx_ + 1) % RING_SIZE;
    if (ring_count_ < RING_SIZE) ring_count_++;

    // Store latest temps (not averaged — they change slowly)
    latest_cpu_temp_c_ = read_cpu_temp_wmi();
    latest_gpu_temp_c_ = gpu_temp;
#endif
}

void SystemSource::emit() {
    if (ring_count_ == 0) return;

    // Average the ring buffer
    auto avg = [this](const float *ring, int count) -> float {
        double s = 0;
        for (int i = 0; i < count; i++) s += ring[i];
        return (float)(s / count);
    };

    SystemMessageBinary m{};
    m.timestamp_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    m.cpu_x10          = (uint16_t)(avg(ring_cpu_, ring_count_) * 10);
    m.gpu_x10          = (uint16_t)(avg(ring_gpu_, ring_count_) * 10);
    m.disk_io_x10      = (uint16_t)(avg(ring_disk_, ring_count_) * 10);
    m.net_down_mbps_x10= (uint16_t)(avg(ring_net_down_, ring_count_) * 10);
    m.net_up_mbps_x10  = (uint16_t)(avg(ring_net_up_, ring_count_) * 10);

#ifdef _WIN32
    // RAM (instantaneous, no averaging needed)
    MEMORYSTATUSEX mi{}; mi.dwLength = sizeof(mi);
    if (GlobalMemoryStatusEx(&mi)) {
        m.ram_used_mb  = (uint16_t)((mi.ullTotalPhys - mi.ullAvailPhys) / (1024 * 1024));
        m.ram_total_mb = (uint16_t)(mi.ullTotalPhys / (1024 * 1024));
    }
    m.cpu_temp_x10 = (uint16_t)(latest_cpu_temp_c_ * 10);
    m.gpu_temp_x10 = (uint16_t)(latest_gpu_temp_c_ * 10);
#endif

    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_ = m;
        has_data_ = true;
        dirty_ = true;
    }

    spdlog::debug("System(avg {} samples): CPU={:.1f}%/{:.0f}C GPU={:.1f}%/{:.0f}C RAM={}/{}MB DiskIO={:.0f}% NET={:.1f}/{:.1f}Mbps",
                  ring_count_, m.cpu_x10/10.0, m.cpu_temp_x10/10.0, m.gpu_x10/10.0, m.gpu_temp_x10/10.0,
                  m.ram_used_mb, m.ram_total_mb, m.disk_io_x10/10.0,
                  m.net_down_mbps_x10/10.0, m.net_up_mbps_x10/10.0);
}
