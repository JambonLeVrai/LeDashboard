# Navigation System Specification

## Overview

This document specifies the **navigation system** for the ESP32 dashboard, including screen management, transitions, and user interaction patterns.

---

## Screen Management Architecture

### Screen Hierarchy

```
Root Screen (Default Display)
└── Screen Manager
    ├── Home Screen (Main)
    ├── Feature Screens (Weather, Media, Portfolio, Notifications)
    └── Settings Screen
        
Overlays (on top of current screen):
├── Quick Actions Overlay
├── Notification Popup
└── Loading Indicator
```

### Screen Types

| Type | Description | Example | Persistence |
|------|-------------|---------|-------------|
| **Root** | Background layer | - | Always |
| **Main** | Primary screens | Home, Settings | Created once |
| **Feature** | Detailed screens | Weather Details | Created on demand |
| **Overlay** | Modal popups | Quick Actions | Temporary |
| **Popup** | Notification | Email Alert | Temporary |

---

## Screen Manager

### Core Functionality

- Maintains a stack of active screens
- Handles screen creation, display, and destruction
- Manages transitions between screens
- Provides back navigation

### API

```cpp
class ScreenManager {
public:
    void registerScreen(const char* name, std::function<lv_obj_t*()> creator);
    void showScreen(const char* name, AnimationType anim = NONE);
    void goBack();
    const char* getCurrentScreenName() const;
    size_t getScreenStackSize() const;
    void showOverlay(const char* name);
    void hideOverlay();
    void cleanup();
};
```

### Screen Stack Behavior

- **Push**: New screen added to stack, animated in
- **Pop**: Current screen removed, previous screen shown
- **Replace**: Current screen replaced (no history)
- **Maximum Depth**: 5 screens deep

---

## Screen Definitions

### Home Screen (Default)

**Layout**: 3x2 grid of widgets (240x150 each)

```
Row 1: [Time] [Weather]
Row 2: [Media] [Portfolio]
Row 3: [Notifications] [System]
Footer: Navigation Bar
```

**Behavior**:
- Auto-refreshes all widgets periodically
- Tap on any widget to navigate to detail screen
- Swipe left/right to navigate to adjacent screens

### Weather Screen

**Layout**:
- Current weather (large, 96pt temp + icon)
- Hourly forecast (horizontal scroll)
- Daily forecast (grid)
- Weather details (humidity, wind, etc.)

**Navigation**:
- From Home: Tap Weather widget
- Back to Home: Swipe right or tap Back

### Media Screen

**Layout**:
- Album art (200x200, centered)
- Track info (title, artist, album)
- Progress bar + time display
- Playback controls (prev, play/pause, next)
- Volume control (slider)

**Navigation**:
- From Home: Tap Media widget
- Back to Home: Swipe right or tap Back

### Portfolio Screen

**Layout**:
- Total value (48pt)
- Today's change (24pt, green/red)
- Performance chart (sparkline)
- Account summary (table)
- Top holdings (list)
- Options positions (list)

**Behavior**:
- Scrollable content
- Tap on holdings for details
- Pull down to refresh

### Notifications Screen

**Layout**:
- Tabs: All, Email, Apps, System
- Notification list (scrollable)
- Each item: icon, title, preview, timestamp

**Behavior**:
- Tap notification to view details
- Swipe left on notification to delete
- Mark all as read button

### Settings Screen

**Layout**:
- Categories (Display, Network, Features, About)
- Settings items per category
- Save button at bottom

**Categories**:
- **Display**: Brightness, Theme, Screen timeout
- **Network**: WiFi, Host IP, Port
- **Features**: Enable/disable each feature
- **About**: Version, Device info, Reset

---

## Navigation Bar (Footer)

### Design

- **Position**: Bottom of screen (60px height)
- **Width**: Full screen width (480px)
- **Background**: Light gray (#F5F5F5)
- **Buttons**: 5 equally sized (96px each)

### Buttons

| Index | Icon | Label | Action |
|-------|------|-------|--------|
| 0 | Home | Home | Show Home |
| 1 | Weather | Weather | Show Weather |
| 2 | Media | Media | Show Media |
| 3 | Notifications | Notifications | Show Notifications |
| 4 | Settings | Settings | Show Settings |

### Active State

- Active button: Blue background (#4285F4)
- Inactive buttons: Transparent
- Notifications button: Shows unread badge (red circle with count)

### Implementation

```cpp
class NavBar {
public:
    enum Button { HOME, WEATHER, MEDIA, NOTIFICATIONS, SETTINGS };
    
    NavBar(lv_obj_t* parent);
    void setCallback(std::function<void(int)> callback);
    void setActiveButton(Button button);
    void setBadgeCount(int count);
};
```

---

## Status Bar (Header)

### Design

- **Position**: Top of screen (60px height)
- **Width**: Full screen width (480px)
- **Background**: Primary color (#4285F4)
- **Text**: White

### Left Section (Time)

- Time: 20pt font, top line
- Date: 14pt font, bottom line
- Format: "12:34" / "Monday, Oct 1"

### Right Section (System)

- Battery icon + percentage
- WiFi icon + signal strength
- Spacing: 8px between icons

### Implementation

```cpp
class StatusBar {
public:
    StatusBar(lv_obj_t* parent, DataStore& dataStore);
    void update();
};
```

---

## Overlays

### Quick Actions Overlay

**Design**:
- Semi-transparent black background (160px height)
- Slides up from bottom
- 6 action buttons in 2x3 grid
- Close button (top-right)

**Actions**:
1. Play/Pause media
2. Next track
3. Previous track
4. Adjust brightness
5. Lock screen
6. Reboot ESP32

**Behavior**:
- Swipe up from bottom edge to show
- Swipe down or tap outside to hide
- Auto-hide after 5 seconds of inactivity

### Notification Popup

**Design**:
- White rounded rectangle (240x160)
- Centered on screen
- Semi-transparent black background

**Contents**:
- Header: Icon + title + close button
- Content: Sender, subject, preview
- Auto-hide after 5 seconds

**Behavior**:
- Shows when new notification arrives
- Tap to view details
- Swipe to dismiss

### Loading Indicator

**Design**:
- Semi-transparent black background
- White rounded rectangle (160x100)
- Spinner animation (60x60)
- "Loading..." text

**Behavior**:
- Shows when waiting for data from host
- Hide when data is received or timeout

---

## Gesture Support

### Touch Gestures

| Gesture | Direction | Action |
|---------|-----------|--------|
| **Swipe Left** | Left | Previous screen |
| **Swipe Right** | Right | Next screen / Go back |
| **Swipe Up** | Up | Show quick actions |
| **Swipe Down** | Down | Hide overlay / Scroll down |
| **Tap** | - | Activate element |
| **Long Press** | - | Show context menu |
| **Double Tap** | - | Quick action |

### Thresholds

- **Swipe**: 50px minimum movement
- **Long Press**: 500ms hold
- **Double Tap**: 300ms between taps
- **Touch Target**: Minimum 48x48px

### Implementation

```cpp
class GestureController {
public:
    enum Gesture { SWIPE_LEFT, SWIPE_RIGHT, SWIPE_UP, SWIPE_DOWN, TAP, LONG_PRESS, DOUBLE_TAP };
    
    GestureController(lv_obj_t* parent);
    void setCallback(std::function<void(Gesture)> callback);
    void enable();
    void disable();
};
```

---

## Screen Transitions

### Animation Types

| Type | Duration | Direction | Use Case |
|------|----------|-----------|----------|
| **Slide Left** | 300ms | Left | Forward navigation |
| **Slide Right** | 300ms | Right | Back navigation |
| **Slide Up** | 300ms | Up | Modal show |
| **Slide Down** | 300ms | Down | Modal hide |
| **Fade In** | 200ms | - | Overlay show |
| **Fade Out** | 200ms | - | Overlay hide |

### Easing Functions

- **Linear**: Consistent speed
- **Ease In**: Slow start, fast end
- **Ease Out**: Fast start, slow end
- **Ease In Out**: Slow start and end
- **Overshoot**: Bounces slightly
- **Bounce**: Bounces multiple times

### Implementation

```cpp
class ScreenTransitions {
public:
    enum Direction { LEFT, RIGHT, UP, DOWN };
    
    static void slideIn(lv_obj_t* obj, Direction dir, uint32_t time = 300);
    static void slideOut(lv_obj_t* obj, Direction dir, uint32_t time = 300);
    static void fadeIn(lv_obj_t* obj, uint32_t time = 200);
    static void fadeOut(lv_obj_t* obj, uint32_t time = 200);
};
```

---

## Navigation Flow

### Main Navigation

```
Home Screen
├── Tap Weather → Weather Screen
├── Tap Media → Media Screen
├── Tap Portfolio → Portfolio Screen
├── Tap Notifications → Notifications Screen
└── Tap Settings → Settings Screen

All Feature Screens
├── Swipe Right → Home Screen
├── Tap Nav Bar → Corresponding Screen
└── Swipe Left → Next Screen (cyclic)
```

### Settings Navigation

```
Settings Screen
├── Tap Category → Expand/Collapse
├── Tap Setting → Edit
├── Tap Save → Save and return to Home
└── Tap Back → Home Screen
```

### Overlay Navigation

```
Any Screen
├── Swipe Up → Show Quick Actions
│   └── Tap Action → Execute action
│   └── Swipe Down → Hide
│   
└── Notification Arrives → Show Popup
    └── Auto-hide after 5s or tap
```

---

## Best Practices

1. **Memory Management**
   - Reuse screens instead of recreating
   - Delete screens when no longer needed
   - Limit screen stack depth

2. **Performance**
   - Use hardware-accelerated animations when available
   - Limit number of concurrent animations
   - Prefer simple transitions on low-end hardware

3. **User Experience**
   - Always provide a way back
   - Consistent navigation patterns
   - Clear visual feedback
   - Responsive to touch input

4. **Error Handling**
   - Handle out-of-memory situations
   - Provide fallback for unsupported features
   - Graceful degradation on low resources

---

## Implementation Checklist

- [ ] ScreenManager class
- [ ] HomeScreen implementation
- [ ] WeatherScreen implementation
- [ ] MediaScreen implementation
- [ ] PortfolioScreen implementation
- [ ] NotificationsScreen implementation
- [ ] SettingsScreen implementation
- [ ] NavBar implementation
- [ ] StatusBar implementation
- [ ] QuickActionsOverlay implementation
- [ ] NotificationPopup implementation
- [ ] LoadingIndicator implementation
- [ ] GestureController implementation
- [ ] ScreenTransitions implementation
- [ ] Integration with main application loop
- [ ] Hardware testing

---

## Summary

This navigation system provides a comprehensive framework for managing the ESP32 dashboard UI. It supports:

- Hierarchical screen management
- Smooth transitions between screens
- Intuitive touch gestures
- Overlay popups for quick actions
- Status and navigation bars
- Memory-efficient screen handling

All components are designed to work within the 480x480 display constraints while providing a modern, responsive user experience.
