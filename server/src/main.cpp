// LeDashboard host server - entry point.
//
// Usage:
//   ledashboard_server.exe [--port COM5] [--baud 115200] [--config config.json]
//
// Defaults: port=COM3, baud=115200 (can be overridden by config.json or CLI args)
//
// Build:
//   cmake --preset windows-ninja
//   cmake --build --preset windows-ninja
// Run:
//   .\build-ninja\bin\ledashboard_server.exe --port COM5

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <lz4.h>

#include "server.h"

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <csignal>
#include <atomic>

static std::atomic<bool> g_interrupted{false};

static void signal_handler(int) {
    g_interrupted = true;
}

struct Config {
    std::string port = "COM3";
    uint32_t baud_rate = 115200;
    uint32_t update_interval_ms = 100;
    std::string log_level = "info";
    std::string weather_location = "Paris";  // city name or "lat,lon"
};

static Config load_config(int argc, char *argv[]) {
    Config cfg;

    // Try to load config.json from the working directory or alongside the exe
    std::vector<std::string> config_paths = {
        "config.json",
        "server/config.json",
        "../server/config.json",
    };

    nlohmann::json j;
    bool have_json = false;
    for (const auto &path : config_paths) {
        std::ifstream f(path);
        if (f) {
            try {
                j = nlohmann::json::parse(f);
                have_json = true;
                spdlog::info("Loaded config from {}", path);
                break;
            } catch (const std::exception &e) {
                spdlog::warn("Failed to parse {}: {}", path, e.what());
            }
        }
    }

    if (have_json) {
        if (j.contains("server")) {
            auto &s = j["server"];
            if (s.contains("port"))         cfg.port = s["port"].get<std::string>();
            if (s.contains("baud_rate"))    cfg.baud_rate = s["baud_rate"].get<uint32_t>();
            if (s.contains("update_interval_ms")) cfg.update_interval_ms = s["update_interval_ms"].get<uint32_t>();
            if (s.contains("log_level"))    cfg.log_level = s["log_level"].get<std::string>();
        }
        if (j.contains("weather")) {
            auto &w = j["weather"];
            if (w.contains("location")) cfg.weather_location = w["location"].get<std::string>();
        }
    }

    // CLI args override config.json
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            cfg.port = argv[++i];
        } else if ((arg == "--baud" || arg == "-b") && i + 1 < argc) {
            cfg.baud_rate = std::stoul(argv[++i]);
        } else if (arg == "--city" && i + 1 < argc) {
            cfg.weather_location = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            std::ifstream f(argv[++i]);
            if (f) {
                auto cj = nlohmann::json::parse(f);
                if (cj.contains("server")) {
                    auto &s = cj["server"];
                    if (s.contains("port"))      cfg.port = s["port"].get<std::string>();
                    if (s.contains("baud_rate")) cfg.baud_rate = s["baud_rate"].get<uint32_t>();
                }
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: ledashboard_server [--port COMx] [--baud N] [--city \"City Name\"] [--config file.json]\n";
            std::cout << "  Default port: COM3, baud: 115200, city: Paris\n";
            std::cout << "  Weather: Open-Meteo (no API key needed)\n";
            std::exit(0);
        }
    }

    return cfg;
}

int main(int argc, char *argv[]) {
    // Set up colored console logging
    auto console = spdlog::stdout_color_mt("console");
    console->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    Config cfg = load_config(argc, argv);
    console->set_level(spdlog::level::from_str(cfg.log_level));

    spdlog::info("LeDashboard server v1.0.0 starting");
    spdlog::info("LZ4 runtime version: {}", LZ4_versionString());
    spdlog::info("Config: port={} baud={} interval={}ms log_level={} city={}",
                 cfg.port, cfg.baud_rate, cfg.update_interval_ms, cfg.log_level, cfg.weather_location);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    DashboardServer server;
    if (!server.initialize(cfg.port, cfg.baud_rate, cfg.weather_location)) {
        spdlog::error("Failed to initialize server on {}", cfg.port);
        std::cerr << "Failed to open serial port " << cfg.port << std::endl;
        std::cerr << "Available ports can be listed with: powershell \"[System.IO.Ports.SerialPort]::GetPortNames()\"" << std::endl;
        return 1;
    }

    // Run server in a thread so we can handle Ctrl+C
    std::thread server_thread([&server]() { server.run(); });

    spdlog::info("Server running on {}. Press Ctrl+C to stop.", cfg.port);

    while (!g_interrupted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    spdlog::info("Shutting down...");
    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    spdlog::info("Server stopped.");
    return 0;
}
