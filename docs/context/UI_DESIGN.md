# UI Design Specification for ESP32 Dashboard

## Overview

This document specifies the **LVGL-based user interface design** for the ESP32 dashboard. The UI is designed to be responsive, touch-friendly, and efficient on the 480x480 display.

---

## Screen Specifications

### Display Characteristics
- **Resolution**: 480x480 pixels (square)
- **Color Depth**: 16-bit (65,536 colors) or 24-bit
- **Touch**: Resistive or Capacitive
- **Orientation**: Portrait (default) or Landscape

### Design Grid
- **Columns**: 2 columns of 240px each
- **Rows**: 3 main rows of 160px each
- **Header**: 60px at top (status bar)
- **Footer**: 60px at bottom (navigation)
- **Margins**: 8px between elements
- **Touch Target**: Minimum 48x48px

---

## Color Scheme

### Light Theme
| Element | Color | Hex |
|---------|-------|-----|
| Background | White | `#FFFFFF` |
| Primary | Blue | `#4285F4` |
| Secondary | Light Gray | `#F5F5F5` |
| Text Primary | Black | `#000000` |
| Text Secondary | Dark Gray | `#616161` |
| Accent | Green | `#0F9D58` |
| Warning | Orange | `#FF9800` |
| Error | Red | `#DB4437` |

### Dark Theme
| Element | Color | Hex |
|---------|-------|-----|
| Background | Dark Gray | `#121212` |
| Primary | Light Blue | `#8AB4F8` |
| Text Primary | White | `#FFFFFF` |
| Text Secondary | Light Gray | `#B0B0B0` |

---

## Typography

### Font Sizes
| Size | Usage | LVGL Font |
|------|-------|-----------|
| 8pt | Tiny labels | `lv_font_montserrat_8` |
| 12pt | Small text | `lv_font_montserrat_12` |
| 16pt | Body text | `lv_font_montserrat_16` |
| 20pt | Headings | `lv_font_montserrat_20` |
| 24pt | Large text | `lv_font_montserrat_24` |
| 32pt | Time display | `lv_font_montserrat_32` |
| 48pt | Large numbers | `lv_font_montserrat_48` |
| 64pt | Extra large | `lv_font_montserrat_64` |

---

## Screen Hierarchy

1. **Home Screen** - Primary dashboard
2. **Feature Screens** - Detailed views (Weather, Media, Portfolio, Notifications)
3. **Settings Screen** - Configuration
4. **Quick Actions Overlay** - Common actions

---

## Home Screen Layout

```
+---------------------------------------------------+
|  [12:34]  [22°C Sunny]  [Bat: 85%]  [WiFi: 3/4]    |  <- Header (60px)
+---------------------------------------------------+
|  +---------------------+  +---------------------+ |  <- Row 1 (160px)
|  |     TIME           |  |     WEATHER        | |
|  |  12:34:56          |  |  22°C, Sunny        | |
|  |  Monday, Oct 1     |  |  H:65%, W:12km/h   | |
|  +---------------------+  +---------------------+ |
+---------------------------------------------------+
|  +---------------------+  +---------------------+ |  <- Row 2 (160px)
|  |     MEDIA          |  |    PORTFOLIO       | |
|  |  [Album Art]       |  |  $125,432.50       | |
|  |  Song Title       |  |  +$456.75 (+0.36%) | |
|  |  [Progress Bar]   |  |  [Mini Chart]     | |
|  +---------------------+  +---------------------+ |
+---------------------------------------------------+
|  +---------------------+  +---------------------+ |  <- Row 3 (160px)
|  |   NOTIFICATIONS    |  |    SYSTEM           | |
|  |  3 New Emails      |  |  CPU: 25%, Mem: 60%| |
|  |  From: John        |  |  Disk: 45%         | |
|  |  [Email Icon]     |  |  [CPU Icon]        | |
|  +---------------------+  +---------------------+ |
+---------------------------------------------------+
|  [Home] [Weather] [Media] [Notif] [Settings]       |  <- Footer (60px)
+---------------------------------------------------+
```

---

## Feature Screen Layouts

### Weather Screen
- Current weather (large temperature + icon)
- Hourly forecast (scrollable)
- Daily forecast
- Weather details (humidity, wind, pressure, etc.)

### Media Screen
- Album art (200x200)
- Track info (title, artist, album)
- Progress bar + time display
- Playback controls (prev, play/pause, next)
- Volume control

### Portfolio Screen
- Total portfolio value (48pt)
- Today's change (green/red, 24pt)
- Performance sparkline chart
- Account summary
- Top holdings list
- Options positions

### Notifications Screen
- Tabs (All, Email, Apps, System)
- Notification list (scrollable)
- Each notification: icon, title, preview, timestamp
- Mark all as read button

### Settings Screen
- Organized by categories
- Toggle switches for features
- Sliders for numeric values
- Dropdowns for selections
- Save/Reset/About buttons

---

## Widget Specifications

### Basic Widgets (LVGL)
- **Label**: Text display
- **Button**: Interactive button
- **Image**: Icon or image display
- **Bar**: Progress bar
- **Arc**: Gauge/circular progress
- **Chart**: Line/bar charts
- **List**: Scrollable list
- **Switch**: Toggle
- **Slider**: Value adjustment
- **Dropdown**: Selection
- **Textarea**: Text input

### Custom Widgets
1. **Weather Widget** (240x150)
   - Temperature (64pt)
   - Condition (20pt)
   - Icon (64x64)
   - Details grid (humidity, wind, etc.)

2. **Media Widget** (240x150)
   - Album art (120x120)
   - Title (20pt)
   - Artist (16pt)
   - Progress bar
   - Time display

3. **Portfolio Widget** (240x150)
   - Total value (48pt)
   - Change amount (24pt)
   - Change percentage (20pt)
   - Mini sparkline chart

4. **Notification Widget** (240x150)
   - Badge with count (32x32)
   - Title (16pt)
   - Sender (14pt)
   - Preview text (14pt)
   - Timestamp (12pt)
   - Icon (24x24)

5. **System Widget** (240x150)
   - CPU gauge (80x80 arc)
   - Memory gauge (80x80 arc)
   - Disk gauge (80x80 arc)
   - Network speed

---

## Navigation System

### Navigation Bar (Footer)
- 5 buttons: Home, Weather, Media, Notifications, Settings
- Each button: 80x60px with icon
- Notifications button has unread badge

### Screen Transitions
- **Slide**: Left/right for feature screens
- **Fade**: For overlays and popups
- **None**: For status bar updates

### Gestures
- **Swipe Left/Right**: Navigate between screens
- **Swipe Up**: Show quick actions
- **Swipe Down**: Hide quick actions
- **Long Press**: Show context menu
- **Double Tap**: Quick action

---

## Status Bar (Header)

**Contents**:
- Time (20pt) - top-left
- Date (14pt) - below time
- Battery icon + percentage - top-right
- WiFi icon + strength - right of battery

**Size**: 40px height (can expand to 60px for date)

---

## Quick Actions Overlay

**Design**: Semi-transparent overlay (80% opacity black background)

**Contents**:
- Title: "Quick Actions"
- Grid of 6 action buttons (2x3)
- Each button: Icon + label
- Close button (top-right)

**Show/Hide**: Swipe up from bottom to show, swipe down or tap outside to hide

**Actions**:
- Media controls (play/pause, next, previous)
- Screen brightness
- Lock screen
- Reboot ESP32
- Open settings
- Run custom script

---

## Notification Popup

**Design**: White rounded rectangle (240x160)

**Contents**:
- Header: Icon + title + close button
- Content: Sender, subject, preview
- Optional: Action buttons

**Behavior**:
- Auto-show for new notifications
- Auto-hide after 5 seconds
- Tap to view details
- Swipe to dismiss

---

## Loading Indicator

**Design**: Semi-transparent black overlay with:
- Spinner animation (60x60)
- "Loading..." text

**Show/Hide**: When waiting for data from host

---

## Context Menu

**Design**: White rounded rectangle with list of actions

**Show/Hide**: Long press on touchable elements

**Contents**: Action-specific options (e.g., for notification: Mark as read, Delete, Reply)

---

## Animations

### Types
1. **Fade**: For screen transitions (200ms)
2. **Slide**: For navigation (300ms)
3. **Scale**: For button press feedback (100ms out, 150ms in)
4. **Progress**: For progress bars (500ms)
5. **Spinner**: For loading indicator

### Easing Functions
- `lv_anim_path_linear`: Linear
- `lv_anim_path_ease_in`: Ease in
- `lv_anim_path_ease_out`: Ease out
- `lv_anim_path_ease_in_out`: Ease in out
- `lv_anim_path_overshoot`: Overshoot
- `lv_anim_path_bounce`: Bounce

---

## Theming

### Theme System
- **Light Theme**: Default
- **Dark Theme**: For low-light environments
- **Custom Themes**: User-configurable colors

### Theme Configuration
```c
lv_theme_t *theme = lv_theme_default_init(NULL, theme, 
    LV_THEME_DEFAULT_COLOR_PRIMARY, 
    LV_THEME_DEFAULT_COLOR_SECONDARY,
    LV_THEME_DEFAULT_FONT_SMALL,
    LV_THEME_DEFAULT_FONT_NORMAL);
```

---

## Performance Optimization

### Rendering
- **Double Buffering**: Enable in LVGL config
- **Non-OPAs**: Mark widgets as non-opaque
- **GPU Acceleration**: Use if available
- **Avoid Overlapping**: Minimize widget overlap
- **Limit Animations**: Only animate visible elements

### Memory
- **Reuse Widgets**: Create once, update data
- **Static Text**: For text that doesn't change
- **Image Cache**: Limit cache size
- **Compiled Images**: Use C arrays instead of files

### Updates
- **Conditional**: Only update when data changes
- **Batch**: Group multiple updates
- **Throttle**: Limit update frequency

---

## Touch Input

### Calibration
- **Points**: 4 corners for accurate calibration
- **Storage**: Save calibration data to flash
- **Recovery**: Factory reset option

### Gesture Detection
- **Tap**: Quick press and release
- **Long Press**: Press >500ms
- **Swipe**: Move >50px in any direction
- **Double Tap**: Two quick taps
- **Pinch**: Two-finger zoom (if supported)

---

## Image Assets

### Required Icon Set

**Weather Icons** (32x32, 64x64, 96x96):
- sun, cloud, rain, snow, thunder, fog, partly_cloudy, windy

**Media Icons** (24x24, 48x48):
- play, pause, previous, next, volume_up, volume_down, volume_mute, repeat, shuffle

**Notification Icons** (24x24, 32x32):
- email, app, system, calendar

**System Icons** (24x24, 32x32):
- battery_0 to battery_100, wifi_0 to wifi_4, cpu, memory, disk, network

**Settings Icons** (24x24, 32x32):
- display, sound, network, about, help

**Navigation Icons** (24x24, 48x48):
- home, weather, media, notifications, settings

**Action Icons** (24x24):
- check, close, arrow_left, arrow_right, arrow_up, arrow_down

### Image Conversion
```bash
# Convert PNG to C array for LVGL
lv_imgconv --in icon.png --out icon.c --name icon_name
```

---

## Best Practices

### Layout
1. Use grid layout for alignment
2. Maintain consistent margins (8px)
3. Create clear visual hierarchy
4. Group related elements
5. Align to 8px grid

### Colors
1. Ensure sufficient contrast
2. Use consistent color scheme
3. Convey meaning with colors
4. Support colorblind users
5. Support dark mode

### Typography
1. Use font sizes to create hierarchy
2. Ensure readability at all sizes
3. Use consistent font family
4. Align text properly
5. Handle long text (truncate or wrap)

### Touch
1. Minimum 48x48px touch targets
2. Adequate spacing between targets
3. Visual feedback on press
4. Support common gestures
5. Provide calibration

### Performance
1. Lazy load images
2. Cache resources
3. Reuse widgets
4. Limit animations
5. Throttle updates

---

## File Structure

See related files for detailed implementations:
- [UI_WIDGETS.md](./UI_WIDGETS.md) - Widget implementations
- [UI_NAVIGATION.md](./UI_NAVIGATION.md) - Navigation system
- [UI_THEMES.md](./UI_THEMES.md) - Theming system
- [UI_ANIMATIONS.md](./UI_ANIMATIONS.md) - Animation examples

---

## Summary

This UI design provides a comprehensive framework for the ESP32 dashboard. For implementation:

1. Set up LVGL with proper configuration
2. Create the screen manager and navigation
3. Implement the status bar and navigation bar
4. Build feature screens using widgets
5. Add animations for smooth transitions
6. Implement touch handling
7. Optimize for performance
8. Test on target hardware
