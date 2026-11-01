#pragma once

#include <string>
#include <cstdint>
#include <vector>

// =============================================================================
// Auto-discovery: scan all COM ports, send DISCOVER probe, find the ESP32
// =============================================================================

// Try to find the LeDashboard peripheral on any available COM port.
// Sends MSG_DISCOVER with magic payload to each port, waits for MSG_DISCOVER_ACK.
// Returns the COM port name (e.g. "COM4") if found, empty string otherwise.
// timeout_per_port_ms: how long to wait for a response on each port (default 500ms)
std::string discover_peripheral(uint32_t baud_rate = 115200, int timeout_per_port_ms = 500);

// List all available COM port names (e.g. {"COM1", "COM3", "COM4", ...})
std::vector<std::string> list_com_ports();
