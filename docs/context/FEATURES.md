# Dashboard Features Specification

## Overview

This document specifies the **feature modules** for the ESP32-4848S040 Dashboard project with C++ host application, **optimized for ESP32 CPU responsiveness**.

**Core Design Principles**:
- **Combined Time & Weather** as primary page with **French formatting** (24h clock, day of week)
- **Media Player** as persistent overlay at bottom (50px collapsed, 240px expanded)
- **Page navigation** via tap on left/right screen edges (circular)
- **Notifications** as non-blocking popups
- **Read-only display** (no playback controls initially)
- **LZ4 compression** for minimal ESP32 CPU load
- **JSON/Binary format constants** defined in both firmware and host code
- **Host-managed configuration** (no settings UI on device)

---

## 1. Time & Weather Combined Module

### Overview
**Primary dashboard page** (Page 1, default) combining time, date, and weather in a unified layout with **French formatting**.

### Layout

```
┌─────────────────────────────────────────────────────────┐
│                                                             │
│  ┌─────────────────────────┬─────────────────────────┐  │
│  │                         │                         │  │
│  │  Jeudi 24 octobre 2026    │                         │  │
│  │                             │    ☀️                   │  │
│  │      13:40               │                         │  │
│  │                             │   15°C / 8°C            │  │
│  │    ☁️                    │    (Demain)             │  │
│  │   22°C                  │                         │  │
│  │   RH: 65%  QA: Bon       │                         │  │
│  │   UV: 3                 │                         │  │
│  │                         │                         │  │
│  └─────────────────────────┴─────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────┘
```

### Left Column (Time & Current Weather)

| Element | Format | Example | Size | Alignment |
|---------|--------|---------|------|------------|
| Date | `EEEE d MMMM yyyy` | "Jeudi 24 octobre 2026" | 20pt | Left |
| Time | `HH:mm` | "13:40" | 64pt | Left |
| Weather icon | - | ☁️ | 64x64 | Left (below time) |
| Current temp | `N°C` | "22°C" | 48pt | Left (next to icon) |
| Humidity | `RH: N%` | "RH: 65%" | 16pt | Left |
| Air Quality | `QA: Text` | "QA: Bon" | 16pt | Left |
| UV Index | `UV: N` | "UV: 3" | 16pt | Left |

**Layout**: All left-aligned, arranged vertically.

### Right Column (Tomorrow Forecast)

| Element | Format | Example | Size | Alignment |
|---------|--------|---------|------|------------|
| Weather icon | - | ☀️ | 96x96 | Right |
| Temperature range | `N°C / N°C` | "15°C / 8°C" | 24pt | Right |
| Label | - | "Demain" | 16pt | Right |

**Layout**: All right-aligned, centered vertically.

### French Localization

**Days of Week**: lundi, mardi, mercredi, jeudi, vendredi, samedi, dimanche

**Months**: janvier, février, mars, avril, mai, juin, juillet, août, septembre, octobre, novembre, décembre

**Air Quality Mapping**:
- 0-50: Bon (Green)
- 51-100: Moyen (Yellow)
- 101-150: Mauvais (Orange)
- 151-200: Très Mauvais (Red)
- 201+: Dangereux (Dark Red)

**UV Index Mapping**:
- 0-2: Faible (Green)
- 3-5: Modéré (Yellow)
- 6-7: Élevé (Orange)
- 8-10: Très Élevé (Red)
- 11+: Extrême (Purple)

### 5-Day Forecast Page

**Access**: Long press on the **lower part of the screen** where weather is displayed (both left and right columns)

**Layout**: Full-screen with 5 daily forecast cards in a single row

```
┌─────────────────────────────────────────────────────────┐
│  [← Retour]              PRÉVISIONS 5 JOURS                 │
│                                                             │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────┐ │
│  │   Lun   │ │   Mar   │ │   Mer   │ │   Jeu   │ │  Ven  │ │
│  │    ☀️    │ │    ☁️    │ │    ☔️    │ │    ❄️    │ │   🌧️   │ │
│  │  18/12°C │ │  15/10°C │ │  14/8°C  │ │   5/0°C  │ │ 16/11°C │ │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────┘ │
│                                                             │
└─────────────────────────────────────────────────────────┘
```

**Each Card Shows**:
- Day abbreviation (Lun, Mar, Mer, Jeu, Ven, Sam, Dim)
- Weather icon
- Min/Max temperature: `{min}°C/{max}°C`

**Navigation**:
- Back button → Return to Time+Weather page
- Edge taps still work for main page navigation

---

## 2. Media Player Module

### Overview
**Persistent overlay** at the bottom of **every page**, always visible. Displays current media information. Currently **read-only** (no playback controls).

### Collapsed State (Default - 50px height)

```
┌─────────────────────────────────────────────────────────┐
│  Artist Name - Song Title          [▰▰▰▯▯▯▯▯]  ▶  2:34/4:56 │
└─────────────────────────────────────────────────────────┘
```

Shows (left to right):
- "Artist Name - Song Title" (truncated to fit available width)
- Progress bar
- Play state icon (▶ for playing, ⏸ for paused) + "MM:SS/MM:SS"

### Expanded State (240px height - lower half of screen)

```
┌─────────────────────────────────────────────────────────┐
│                                                             │
│  ┌────────┐ Artist Name                                   │
│  │        │ Song Title                                    │
│  │  [Art] │ Album Name                                   │
│  │ 120x120│                                              │
│  └────────┘                                              │
│                                                             │
│  ┌───────────────────────────────────────────────────┐  │
│  │ [▰▰▰▰▰▰▰▯▯▯▯▯▯▯▯▯]               ▶  2:34 / 4:56         │  │
│  └───────────────────────────────────────────────────┘  │
│                                                             │
│  Type: Spotify  Format: FLAC  Bitrate: 320 kbps            │
│                                                             │
└─────────────────────────────────────────────────────────┘
```

Shows:
- Album art (120x120, left)
- Artist (20pt), Song title (24pt), Album name (16pt) - stacked vertically right of art
- Full-width progress bar + state icon + time display
- Media metadata (type, format, bitrate)

### Behavior

- **Always visible** at bottom of every page
- **Tap anywhere on bar** → Toggle expand/collapse (200-300ms smooth animation)
- **Overlay** → On top of page content, does **NOT block** interaction with underlying page
- **Read-only** → No playback controls (for now)

### Album Art Handling

**Sources** (in priority order):
1. Player API (Spotify, Windows Media Player, etc.)
2. File metadata (ID3 tags for local files)
3. Online lookup (Last.fm, Cover Art Archive) - optional
4. Fallback: Generic music note icon or artist/album initials

**Caching**:
- **Location**: `/cache/album_art/` on ESP32 flash
- **Max Images**: 20
- **Max Storage**: 5MB total
- **Filename**: `{artist_hash}_{album_hash}.jpg`
- **Cleanup**: Auto-remove oldest when limit reached

**Display**:
- Size: 120x120 pixels
- Position: Left-aligned in expanded view
- Fallback: Show centered artist/album text if no art
- Loading: Fade in animation when loaded

### Future: Playback Controls
When read-only mode is insufficient, add to expanded view:
- Play/Pause button (center)
- Previous/Next buttons (left/right of play)
- Volume slider
- Seek by tapping progress bar

---

## 3. Notification Module

### Overview
**Non-blocking popups** that appear over the current interface to display notifications (email from Thunderbird, system alerts).

### Display

```
┌─────────────────────────────────────────────────────────┐
│  ┌───────────────────────────────────────────────────┐  │
│  │ 📧 Nouveau Message                              X    │  │
│  │──────────────────────────────────────────────────│  │
│  │ De: john@exemple.com                            │  │
│  │ Sujet: Réunion demain à 10h                      │  │
│  │ Aperçu: Avez-vous reçu les documents...        │  │
│  │                                              │  │
│  │     [Lire]     [Supprimer]     [Fermer]             │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### Behavior

| Action | Result |
|--------|--------|
| Appear | Slide up from bottom or fade in (200ms animation) |
| Auto-hide | After 5 seconds (configurable: 3-10s) |
| Dismiss | Tap close button (X) or swipe |
| Tap notification | Mark as read + perform default action |
| New while showing | Queue (show after current hides automatically) |

**Properties**:
- Semi-transparent background overlay (80% opacity black)
- **Non-blocking**: Underlying page content remains interactive
- Smooth animations for show/hide
- Multiple notifications queue automatically

### French Localization

**Labels**: 📧 Nouveau Message, De:, Sujet:, Aperçu:, [Lire], [Supprimer], [Fermer]  
**Timestamp Formats**:
- Same day: "Aujourd'hui à HH:mm"
- Yesterday: "Hier à HH:mm"
- This week: "Jeudi à HH:mm"
- Older: "24 oct. à HH:mm"

---

## 4. Portfolio Module

### Overview
Display financial portfolio data from **Ghostfolio** (self-hosted portfolio management software). Shows summary, accounts, and top holdings.

### Layout

```
┌─────────────────────────────────────────────────────────┐
│ Portfolio: €125 432,50              +€456,75 (+0,36%)     │
│                                                             │
│ COMPTES                                                    │
│ ┌──────────────────┬──────────────────┐                │
│ │ Brokerage         │ Crypto           │                │
│ │ €75 000,00        │ €12 345,67       │                │
│ │ +€300,50 (+0,4%)  │ -€250,89 (-2,0%) │                │
│ └──────────────────┴──────────────────┘                │
│                                                             │
│ TOP VALEURS                                                │
│ 1. AAPL  Apple Inc.     10×€150,25=€1 502,50 +€52,50(+3,62%)│
│ 2. MSFT  Microsoft       5×€246,80=€1 234,00 +€34,00(+2,82%)│
│ 3. GOOGL Alphabet        8×€156,70=€1 253,60 +€87,20(+7,45%)│
│                                                             │
└─────────────────────────────────────────────────────────┘
```

### French Localization

**Labels**: Portfolio, Total, Comptes, Top Valeurs, Symbole, Qté, Prix, Valeur, Gain, Poids  
**Number Format**:
- Thousands separator: Space (e.g., "125 432,50")
- Decimal separator: Comma (e.g., "0,36")
- Percentage: Comma decimal, space before % (e.g., "3,62 %")
- Currency: Symbol before amount (e.g., "€125 432,50")

### Update Frequency
- Summary & accounts: Every 30-60 seconds
- Full sync: Every 2-5 minutes (or on-demand)

---

## 5. System Monitor Module

### Overview
Display host computer system metrics: CPU, memory, storage, and network.

### Layout

```
┌─────────────────────────────────────────────────────────┐
│ MONITEUR SYSTÈME                                          │
│                                                             │
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐        │
│ │    CPU      │ │   MÉMOIRE   │ │   DISQUE C:  │        │
│ │    25 %     │ │    60 %     │ │    45 %     │        │
│ │ 🌡 45 °C    │ │ 8,0/16,0 Go │ │ 220/500 Go  │        │
│ └─────────────┘ └─────────────┘ └─────────────┘        │
│                                                             │
│ RÉSEAU:  ↑ 1,25 Mbps   ↓ 5,60 Mbps                        │
│          Total: ↑ 125 Mo   ↓ 512 Mo                         │
│                                                             │
└─────────────────────────────────────────────────────────┘
```

### French Localization

**Labels**: Moniteur Système, CPU, Mémoire, Disque, Réseau, ↑ (Envoi/Upload), ↓ (Téléchargement/Download), Total, Go, Mo, Mbps, °C  
**Number Format**: Comma decimal, space thousands (e.g., "1,25 Mbps", "8,0 Go", "125 Mo")

### Update Frequency
- CPU & Memory: Every 2 seconds
- Storage: Every 10 seconds
- Network: Every 1 second

---

## Page Navigation System

### Pages

| # | Name | Description | Status |
|---|------|-------------|--------|
| 1 | **Time+Weather** | Default page, combined time & weather | ✅ Active |
| 2 | **Portfolio** | Ghostfolio portfolio data | ✅ Active |
| 3 | **System Monitor** | System metrics (CPU, memory, etc.) | ✅ Active |
| 4 | **Calendar** | Reserved for future Radicale integration | ⏸️ Reserved |

**Special Page**: 5-Day Forecast (accessible from Time+Weather via long press)

### Navigation Method

**Edge Tap Navigation** (circular):
- **Left edge**: Tap anywhere in left 80px zone → **Previous page** (wraps around)
- **Right edge**: Tap anywhere in right 80px zone → **Next page** (wraps around)

**Implementation** (LVGL touch handler):
```cpp
const int EDGE_ZONE_WIDTH = 80;
const int SCREEN_WIDTH = 480;

void on_touch(lv_event_t * e) {
    lv_indev_t * indev = lv_event_get_indev(e);
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    
    if (point.x < EDGE_ZONE_WIDTH) {
        // Left edge: navigate to previous page
        page_manager.goToPreviousPage();
    } else if (point.x > (SCREEN_WIDTH - EDGE_ZONE_WIDTH)) {
        // Right edge: navigate to next page
        page_manager.goToNextPage();
    } else {
        // Center: handle page-specific tap
        current_page->handleTap(point.x, point.y);
    }
}
```

**Circular Navigation**: Pages wrap around
- From Page 1 (Time+Weather), previous → Page 4 (Calendar)
- From Page 4 (Calendar), next → Page 1 (Time+Weather)

### Visual Feedback
- Brief highlight/ripple animation on edge tap
- Smooth slide animation (300ms) for page transitions
- Subtle indicator for current page

### Special Navigation

**From Time+Weather Page**:
- **Long press** on weather area (lower part of screen, both columns) → Navigate to **5-Day Forecast Page**

**From 5-Day Forecast Page**:
- **Back button** (top-left) → Return to Time+Weather page

### Overlay Behavior

**Media Player**:
- Present on **ALL pages**
- Always at bottom (50px collapsed or 240px expanded)
- Overlay on top of page content
- Does **NOT** affect page navigation
- Touch on bar → Toggle expand/collapse
- Underlying content remains **fully interactive**

**Notifications**:
- Popup over current page
- **Non-blocking** (underlying content still interactive)
- Auto-hide after 5 seconds
- Dismiss with close button or swipe

### Navigation Diagram

```
┌─────────┐     ┌──────────┐     ┌─────────┐     ┌─────────┐
│  Time+   │────▶│  Portfolio│────▶│ System  │────▶│ Calendar│
│ Weather  │◀────│           │◀────│ Monitor  │◀────│         │
└─────────┘     └──────────┘     └─────────┘     └─────────┘
      ▲                                                ↓
      └──────────────────────────────────────────────┘
                    (Circular Navigation)

Special:
  Time+Weather ─[long press weather]─▶ 5-Day Forecast
  5-Day Forecast ─[back button]─▶ Time+Weather
```

---

## Removed Features

The following features are **intentionally excluded** to maintain simplicity, focus, and ESP32 responsiveness:

### ❌ Time Module Extras (Use Smartphone)
- **World Clock** - Multiple time zone display
- **Countdown Timer** - User-configurable countdown
- **Stopwatch** - Timing functionality
- **Alarm Indicator** - Visual alarm indicators

**Rationale**: These features are already well-implemented on smartphones. The dashboard focuses on **always-visible information** rather than duplicating phone functionality.

### ❌ Settings Module
**Rationale**: All configuration is managed on the **host computer** via `config.toml` file. ESP32 receives configuration via serial communication and does not store settings locally.

### ❌ Quick Actions Module
**Rationale**: Reduces UI clutter and touch target conflicts. Clean, focused pages with edge-based navigation are preferred.

---

## Communication Protocol

### Key Settings for ESP32 CPU Responsiveness

**Compression Algorithm**: **LZ4**
- **Rationale**: Prioritizes ESP32 CPU efficiency over maximum compression
- **Trade-off**: Slightly larger messages but much faster compression/decompression
- **Typical Ratio**: 40-60% size reduction
- **Implementation**: Compress messages > 200 bytes (configurable threshold)

**Baud Rate**: 115200 (default, configurable to 230400, 460800, 921600)

### Format Constants

As requested, JSON and Binary format selection must be **defined as constants** in both firmware and host to ensure consistency:

**Host (C++ - config.hpp or protocol.hpp)**:
```cpp
namespace Protocol {
    enum class DataFormat : uint8_t {
        JSON = 0,    // For development and debugging
        BINARY = 1   // For production (more efficient)
    };
    
    // Single constant to switch between formats
    constexpr DataFormat FORMAT = DataFormat::BINARY;
    // Or: DataFormat::JSON for development
}
```

**Firmware (C - protocol.h)**:
```c
// Define format options
#define USE_JSON_FORMAT 0
#define USE_BINARY_FORMAT 1

// Single constant to match host setting
#define DATA_FORMAT USE_BINARY_FORMAT

// Use in code with preprocessor
#if DATA_FORMAT == USE_JSON_FORMAT
    // JSON parsing code
    parseJsonMessage(type, payload, length);
#else
    // Binary parsing code
    parseBinaryMessage(type, payload, length);
#endif
```

### Message Types Summary

| Hex | Name | Feature | Direction | Typical Rate |
|-----|------|---------|-----------|--------------|
| 0x00 | HEARTBEAT | Keep-alive ping/pong | Both | 5 seconds |
| 0x01 | TIME | Time and date | Host→ESP32 | 1 second |
| 0x02 | WEATHER | Weather data (current + forecast) | Host→ESP32 | 10-30 minutes |
| 0x03 | MEDIA | Media player info | Host→ESP32 | On change + 1s progress |
| 0x04 | NOTIFICATION | Notifications | Host→ESP32 | Immediate |
| 0x05 | PORTFOLIO | Portfolio data | Host→ESP32 | 30-60 seconds |
| 0x06 | SYSTEM | System monitor data | Host→ESP32 | 1-10 seconds |
| 0x08 | ACK | Positive acknowledgment | Both | On receive |
| 0x09 | NACK | Negative acknowledgment | Both | On error |
| 0x0D | TOUCH | Touch events | ESP32→Host | On touch |

### LZ4 Implementation Notes

**ESP32 Side**:
```cpp
#include <lz4.h>

// Allocate output buffer (must be >= input size for worst case)
uint8_t compressed_buffer[1024];

// Compress data
int compressed_size = LZ4_compress_default(
    (char*)input_data,
    (char*)compressed_buffer,
    input_size,
    sizeof(compressed_buffer)
);

// Decompress data
int decompressed_size = LZ4_decompress_safe(
    (char*)compressed_data,
    (char*)output_buffer,
    compressed_size,
    sizeof(output_buffer)
);

// Check for errors
if (decompressed_size < 0) {
    // Handle decompression error
}
```

**Compression Strategy**:
- Apply LZ4 to payload only (not to frame headers)
- Compress messages where payload > 200 bytes
- Always check if compressed size is smaller before sending
- Fallback to uncompressed if compression doesn't help (rare with LZ4)

---

## Data Size & Bandwidth Estimates

### Message Sizes (Uncompressed, JSON format, typical)

| Feature | Min Size | Typical Size | Max Size |
|---------|----------|--------------|----------|
| Time | 20 B | 50 B | 100 B |
| Weather (current + tomorrow) | 150 B | 300 B | 700 B |
| Weather (5-day forecast) | 200 B | 400 B | 800 B |
| Media (collapsed) | 60 B | 80 B | 120 B |
| Media (expanded, no art) | 200 B | 400 B | 600 B |
| Media (with album art) | 500 B | 1 KB | 2 KB |
| Notifications | 80 B | 150 B | 300 B |
| Portfolio | 100 B | 500 B | 2 KB |
| System Monitor | 60 B | 120 B | 200 B |

### LZ4 Compression Impact

| Original Size | Compressed Size | Ratio | Notes |
|---------------|-----------------|-------|-------|
| 500 B | ~200-300 B | ~40-60% | Typical for JSON/text |
| 1 KB | ~400-600 B | ~40-60% | Good compression |
| 2 KB | ~800-1200 B | ~40-60% | Still effective |

**Note**: LZ4 is extremely fast but provides moderate compression. This is the ideal trade-off for ESP32 where CPU responsiveness is prioritized.

### Bandwidth Estimates

| Scenario | Per Second | Per Minute | Per Hour |
|----------|------------|------------|----------|
| **Typical** (all features, normal update rates) | 500-800 B | 30-50 KB | 2-3 MB |
| **Maximum** (all at maximum update rate) | 2-3 KB | 150-200 KB | 10-12 MB |
| **Minimum** (time + weather only) | 100-200 B | 5-10 KB | 300-600 KB |

**With LZ4 Compression**: Reduce above by ~40-60% for compressible messages

---

## Implementation Priority

### Phase 1: Core MVP (Immediate)
1. ✅ **Time+Weather Page** - Combined layout with French formatting
2. ✅ **Media Player Overlay** - 50px collapsed, 240px expanded states
3. ✅ **Page Navigation** - Edge tap detection, circular navigation
4. ✅ **Communication Protocol** - Serial framing, CRC-16/CCITT, byte stuffing, LZ4 compression
5. ✅ **Host Integration** - Basic data flow for time and weather

### Phase 2: Enhanced Features
6. ✅ **5-Day Forecast Page** - From weather long press
7. ✅ **Notifications** - Popup display with auto-hide
8. ✅ **Portfolio Page** - Ghostfolio integration
9. ✅ **System Monitor Page** - CPU, memory, storage, network metrics

### Phase 3: Future Enhancements
10. ⏸️ **Calendar Page** - When Radicale calendar server is deployed
11. ⏸️ **Media Playback Controls** - When read-only mode is insufficient

---

## Feature Dependencies

```
Time+Weather Module:
    ├─ Host: System clock (Windows API)
    └─ Host: Weather API (OpenWeatherMap or similar)

Media Player Module:
    ├─ Host: Media player monitoring (Spotify API, Windows Media Player COM, etc.)
    └─ Host: Album art fetching and caching

Notification Module:
    └─ Host: Thunderbird mail directory monitoring (file system watch)

Portfolio Module:
    └─ Host: Ghostfolio API (local self-hosted REST API)

System Monitor Module:
    ├─ Host: Windows Performance Counters (CPU, memory)
    ├─ Host: Windows Management Instrumentation (storage, network)
    └─ Host: Custom system monitoring code

Calendar Module (future):
    └─ Host: Radicale API (local self-hosted CalDAV server)
```

---

## Touch Gesture Summary

| Gesture | Trigger Zone | Action | Applies To |
|---------|--------------|--------|------------|
| Tap | Left edge (X: 0-80px) | Previous page | All pages |
| Tap | Right edge (X: 400-480px) | Next page | All pages |
| Tap | Media bar (anywhere) | Toggle expand/collapse | All pages |
| Tap | Forecast back button | Return to Time+Weather | Forecast page |
| Tap | Notification close (X) | Dismiss popup | All pages |
| Tap | Notification content | Mark as read | All pages |
| Long Press | Weather area (lower part, both columns) | Open 5-day forecast | Time+Weather only |
| Swipe | Notification (any direction) | Dismiss | All pages |

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────┐
│              ESP32-4848S040 DASHBOARD - REFERENCE                │
├─────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌───────────────────── HARDWARE ─────────────────────┐   │
│  │  Display: 480x480 RGB, Capacitive Touch                   │   │
│  │  MCU: ESP32 with WiFi/Bluetooth                         │   │
│  │  Serial: UART0 (GPIO1 TX, GPIO3 RX)                     │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── DISPLAY ──────────────────────┐   │
│  │  Locale: French (fr-FR)                                  │   │
│  │  Time Format: 24-hour (HH:mm) e.g., "13:40"                │   │
│  │  Date Format: EEEE d MMMM yyyy e.g., "Jeudi 24 octobre" │   │
│  │  Temperature: Celsius (°C)                              │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── PAGES ────────────────────────┐   │
│  │  1. Time+Weather (DEFAULT)                              │   │
│  │  2. Portfolio                                           │   │
│  │  3. System Monitor                                       │   │
│  │  4. Calendar (reserved)                                 │   │
│  │  Special: 5-Day Forecast (from weather long press)      │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── NAVIGATION ───────────────────┐   │
│  │  Left edge tap (0-80px) → Previous page (circular)       │   │
│  │  Right edge tap (400-480px) → Next page (circular)      │   │
│  │  Weather long press → 5-Day Forecast                    │   │
│  │  Forecast back → Time+Weather                          │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── OVERLAYS ─────────────────────┐   │
│  │  Media: 50px (collapsed) / 240px (expanded)             │   │
│  │    - Always at bottom, on all pages                     │   │
│  │    - Tap to toggle, read-only (no controls)             │   │
│  │  Notifications: Popup, auto-hide 5s, non-blocking        │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── COMMUNICATION ─────────────────┐   │
│  │  Protocol: Custom binary with framing                     │   │
│  │  Baud: 115200 (default, configurable)                    │   │
│  │  Compression: LZ4 (for messages > 200 bytes)             │   │
│  │  Format: Binary (prod) / JSON (debug) via constants      │   │
│  │  CRC: CRC-16/CCITT (0x1021 polynomial)                    │   │
│  │  Byte stuffing: Yes (0xAA, 0x55, 0x5A)                   │   │
│  │  Frame: SOF + Type + Length + Payload + CRC + EOF      │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── UPDATE RATES ───────────────────┐   │
│  │  Time: 1 second                                           │   │
│  │  Weather: 10-30 minutes                                     │   │
│  │  Media: On change + progress every 1 second              │   │
│  │  Portfolio: 30-60 seconds                                  │   │
│  │  System: CPU/Memory 2s, Storage 10s, Network 1s         │   │
│  │  Notifications: Immediate                                │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── MESSAGE TYPES ─────────────────┐   │
│  │  0x00: Heartbeat  0x01: Time  0x02: Weather             │   │
│  │  0x03: Media  0x04: Notification  0x05: Portfolio       │   │
│  │  0x06: System  0x08: ACK  0x09: NACK  0x0D: Touch       │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── REMOVED ───────────────────────┐   │
│  │  ❌ World clock, countdown, stopwatch, alarm            │   │
│  │  ❌ Quick actions, settings UI                           │   │
│  │  (Use smartphone for these features)                     │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌───────────────────── FUTURE ─────────────────────────┐   │
│  │  Calendar page (Radicale integration)                    │   │
│  │  Media playback controls                                  │   │
│  └───────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────┘
```

---

## Summary

This specification defines a **focused, responsive, ESP32-optimized dashboard** with the following key characteristics:

✅ **4 main pages** with circular edge-tap navigation (Time+Weather, Portfolio, System Monitor, Calendar)
✅ **Combined Time+Weather** as default page with **full French formatting** (24h clock, day of week, French month/day names)
✅ **Media Player overlay** at bottom (50px collapsed, 240px expanded), **read-only** mode
✅ **Notification popups** (non-blocking, auto-hide after 5 seconds)
✅ **LZ4 compression** prioritizing ESP32 CPU responsiveness over maximum compression
✅ **JSON/Binary format constants** defined in both firmware (`protocol.h`) and host (`config.hpp`)
✅ **Host-managed configuration** via `config.toml` (no settings UI on device)
✅ **Simplified feature set** - removed redundant features better suited to smartphones

**Design Focus**: Always-visible information display with minimal user interaction, optimized for ESP32 performance constraints.
