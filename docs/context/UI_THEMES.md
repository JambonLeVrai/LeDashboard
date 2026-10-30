# Theming System Specification

## Overview

This document specifies the **theming system** for the ESP32 dashboard, allowing customization of colors, fonts, and visual styles across the entire UI.

---

## Theme Architecture

### Theme Hierarchy

```
┌─────────────────────────────────────────┐
│              Theme Manager                │
├─────────────────────────────────────────┤
│  Active Theme: Light or Dark             │
│       │                                      │
│       ▼                                      │
│  ┌─────────────┐  ┌─────────────┐       │
│  │ Light Theme │  │ Dark Theme   │       │
│  └─────────────┘  └─────────────┘       │
│       │                                      │
│       ▼                                      │
│  ┌─────────────────────────────────────┐ │
│  │           Widget Styles               │ │
│  │  - Colors                               │ │
│  │  - Fonts                                │ │
│  │  - Spacing                              │ │
│  │  - Borders                              │ │
│  └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

### Theme Components

| Component | Description | Scope |
|-----------|-------------|-------|
| **Color Palette** | Primary, secondary, accent colors | Global |
| **Typography** | Font family, sizes, weights | Global |
| **Spacing** | Margins, padding, gaps | Global |
| **Widget Styles** | Button, card, bar, etc. | Per-widget |
| **Custom Styles** | User-defined styles | Per-screen |

---

## Color Palette

### Light Theme

| Name | Hex | Usage |
|------|-----|-------|
| `COLOR_PRIMARY` | `#4285F4` | Buttons, active states, accents |
| `COLOR_PRIMARY_VARIANT` | `#8AB4F8` | Hover states |
| `COLOR_SECONDARY` | `#F5F5F5` | Backgrounds, cards |
| `COLOR_BACKGROUND` | `#FFFFFF` | Main background |
| `COLOR_SURFACE` | `#FFFFFF` | Card/container backgrounds |
| `COLOR_ERROR` | `#DB4437` | Errors, warnings |
| `COLOR_SUCCESS` | `#0F9D58` | Success states, positive values |
| `COLOR_WARNING` | `#FF9800` | Warnings |
| `COLOR_TEXT_PRIMARY` | `#000000` | Main text |
| `COLOR_TEXT_SECONDARY` | `#616161` | Secondary text, hints |
| `COLOR_TEXT_DISABLED` | `#9E9E9E` | Disabled text |
| `COLOR_BORDER` | `#E0E0E0` | Borders, dividers |

### Dark Theme

| Name | Hex | Usage |
|------|-----|-------|
| `COLOR_PRIMARY` | `#8AB4F8` | Buttons, active states |
| `COLOR_PRIMARY_VARIANT` | `#4285F4` | Pressed states |
| `COLOR_SECONDARY` | `#1E1E1E` | Backgrounds, cards |
| `COLOR_BACKGROUND` | `#121212` | Main background |
| `COLOR_SURFACE` | `#1E1E1E` | Card/container backgrounds |
| `COLOR_ERROR` | `#F28B82` | Errors, warnings |
| `COLOR_SUCCESS` | `#81C995` | Success states |
| `COLOR_WARNING` | `#FFAB40` | Warnings |
| `COLOR_TEXT_PRIMARY` | `#FFFFFF` | Main text |
| `COLOR_TEXT_SECONDARY` | `#B0B0B0` | Secondary text |
| `COLOR_TEXT_DISABLED` | `#616161` | Disabled text |
| `COLOR_BORDER` | `#333333` | Borders, dividers |

### Color Usage Guidelines

**Primary Color (#4285F4 / #8AB4F8)**
- Navigation bar active button
- Primary buttons
- Interactive elements
- Accent highlights

**Secondary Color (#F5F5F5 / #1E1E1E)**
- Card backgrounds
- Dialog backgrounds
- Elevated surfaces

**Surface Color (#FFFFFF / #1E1E1E)**
- Main screen background
- Modal backgrounds
- Input fields

**Text Colors**
- Primary: Main content (titles, labels)
- Secondary: Subtitles, hints, placeholders
- Disabled: Unavailable options

---

## Typography

### Font Family

**Primary**: Montserrat (embedded in LVGL)
- Clean, modern sans-serif
- Good readability at small sizes
- Supports all required characters

### Font Sizes

| Size | Usage | LVGL Font | Character Height |
|------|-------|-----------|-------------------|
| Tiny | Small labels, badges | `lv_font_montserrat_8` | 8px |
| Small | Secondary text, timestamps | `lv_font_montserrat_12` | 12px |
| Body | Default text | `lv_font_montserrat_16` | 16px |
| Heading Small | Subtitles | `lv_font_montserrat_20` | 20px |
| Heading Medium | Section titles | `lv_font_montserrat_24` | 24px |
| Heading Large | Screen titles | `lv_font_montserrat_32` | 32px |
| Display Small | Time (seconds) | `lv_font_montserrat_48` | 48px |
| Display Medium | Time (hours:minutes) | `lv_font_montserrat_64` | 64px |
| Display Large | Large numbers | `lv_font_montserrat_96` | 96px |

### Font Weights

| Weight | Usage | LVGL Support |
|--------|-------|---------------|
| Regular | Default | ✓ Yes |
| Medium | Emphasis | ✓ Yes |
| Bold | Headings | ✓ Yes |

### Typography Guidelines

1. **Hierarchy**: Use font sizes to create visual hierarchy
   - Headings: 24-96px
   - Body: 16px
   - Small text: 12-14px

2. **Contrast**: Ensure sufficient contrast with background
   - Light theme: Dark text on light background
   - Dark theme: Light text on dark background

3. **Alignment**: Use appropriate text alignment
   - Titles: Center
   - Body: Left
   - Numbers: Right (when in tables)

4. **Truncation**: Handle long text gracefully
   - Use ellipsis (`...`) for truncation
   - Or wrap text with proper margins

---

## Spacing System

### Base Unit: 8px

All spacing values are multiples of 8px for consistency.

### Spacing Values

| Name | Pixels | Usage |
|------|--------|-------|
| `SPACING_NONE` | 0px | No spacing |
| `SPACING_TINY` | 4px | Minimal spacing |
| `SPACING_SMALL` | 8px | Default spacing |
| `SPACING_MEDIUM` | 16px | Larger spacing |
| `SPACING_LARGE` | 24px | Section spacing |
| `SPACING_XLARGE` | 32px | Major section spacing |

### Spacing Guidelines

**Padding**
- Cards: 16px (SPACING_MEDIUM)
- Buttons: 8-16px (SPACING_SMALL to SPACING_MEDIUM)
- Input fields: 12px vertical, 16px horizontal
- Screens: 8px (SPACING_SMALL) or 0px for full-bleed

**Margins**
- Between elements: 8px (SPACING_SMALL)
- Between sections: 16-24px (SPACING_MEDIUM to SPACING_LARGE)
- Between cards: 8-16px (SPACING_SMALL to SPACING_MEDIUM)

**Gaps**
- Flex/Grid gaps: 8px (SPACING_SMALL)
- Button groups: 8-16px (SPACING_SMALL to SPACING_MEDIUM)

---

## Widget Styles

### Buttons

**Default Button**
```cpp
lv_obj_set_style_bg_color(btn, theme.COLOR_PRIMARY, 0);
lv_obj_set_style_text_color(btn, theme.COLOR_TEXT_PRIMARY, 0);
lv_obj_set_style_radius(btn, 4, 0);
lv_obj_set_style_border_width(btn, 0, 0);
lv_obj_set_style_pad_all(btn, 8, 0);
```

**State Variations**
- **Default**: Primary color background
- **Pressed**: Darker primary color or opacity 200
- **Disabled**: COLOR_BORDER background, COLOR_TEXT_DISABLED text
- **Hover**: COLOR_PRIMARY_VARIANT background (if touch supports hover)

### Cards

```cpp
lv_obj_set_style_bg_color(card, theme.COLOR_SURFACE, 0);
lv_obj_set_style_radius(card, 8, 0);
lv_obj_set_style_border_width(card, 1, 0);
lv_obj_set_style_border_color(card, theme.COLOR_BORDER, 0);
lv_obj_set_style_pad_all(card, 16, 0);
```

### Input Fields

```cpp
lv_obj_set_style_bg_color(input, theme.COLOR_SURFACE, 0);
lv_obj_set_style_border_width(input, 1, 0);
lv_obj_set_style_border_color(input, theme.COLOR_BORDER, 0);
lv_obj_set_style_radius(input, 4, 0);
lv_obj_set_style_pad_horiz(input, 16, 0);
lv_obj_set_style_pad_vert(input, 12, 0);

// Focus state
lv_obj_set_style_border_width(input, 2, LV_STATE_FOCUSED);
lv_obj_set_style_border_color(input, theme.COLOR_PRIMARY, LV_STATE_FOCUSED);
```

### Progress Bars

```cpp
// Background
lv_obj_set_style_bg_color(bar, theme.COLOR_BORDER, LV_PART_MAIN);
lv_obj_set_style_radius(bar, 10, LV_PART_MAIN);

// Indicator
lv_obj_set_style_bg_color(bar, theme.COLOR_PRIMARY, LV_PART_INDICATOR);
lv_obj_set_style_radius(bar, 8, LV_PART_INDICATOR);
```

### Charts

```cpp
lv_obj_set_style_bg_color(chart, theme.COLOR_SURFACE, 0);
lv_obj_set_style_border_color(chart, theme.COLOR_BORDER, 0);
lv_obj_set_style_text_color(chart, theme.COLOR_TEXT_SECONDARY, 0);

// Series colors
lv_color_t seriesColors[] = {
    theme.COLOR_PRIMARY,
    theme.COLOR_SUCCESS,
    theme.COLOR_ERROR,
    theme.COLOR_WARNING
};
```

### Labels

```cpp
// Primary
lv_obj_set_style_text_color(label, theme.COLOR_TEXT_PRIMARY, 0);
lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);

// Secondary
lv_obj_set_style_text_color(label, theme.COLOR_TEXT_SECONDARY, 0);

// Success (positive values)
lv_obj_set_style_text_color(label, theme.COLOR_SUCCESS, 0);

// Error (negative values)
lv_obj_set_style_text_color(label, theme.COLOR_ERROR, 0);
```

---

## Theme Implementation

### Theme Class

```cpp
// theme.hpp
#pragma once
#include <lvgl.h>

class Theme {
public:
    enum Type { LIGHT, DARK, CUSTOM };
    
    struct Colors {
        lv_color_t primary;
        lv_color_t primary_variant;
        lv_color_t secondary;
        lv_color_t background;
        lv_color_t surface;
        lv_color_t error;
        lv_color_t success;
        lv_color_t warning;
        lv_color_t text_primary;
        lv_color_t text_secondary;
        lv_color_t text_disabled;
        lv_color_t border;
    };
    
    Theme(Type type = LIGHT);
    
    void apply();
    void setType(Type type);
    Type getType() const;
    
    const Colors& getColors() const;
    
private:
    Type type_;
    Colors colors_;
    
    void loadLightTheme();
    void loadDarkTheme();
    void loadCustomTheme();
};
```

### Theme Application

```cpp
// theme.cpp
#include "theme.hpp"

Theme::Theme(Type type) : type_(type) {
    if (type == LIGHT) {
        loadLightTheme();
    } else {
        loadDarkTheme();
    }
}

void Theme::loadLightTheme() {
    colors_.primary = lv_color_hex(0x4285F4);
    colors_.primary_variant = lv_color_hex(0x8AB4F8);
    colors_.secondary = lv_color_hex(0xF5F5F5);
    colors_.background = lv_color_hex(0xFFFFFF);
    colors_.surface = lv_color_hex(0xFFFFFF);
    colors_.error = lv_color_hex(0xDB4437);
    colors_.success = lv_color_hex(0x0F9D58);
    colors_.warning = lv_color_hex(0xFF9800);
    colors_.text_primary = lv_color_hex(0x000000);
    colors_.text_secondary = lv_color_hex(0x616161);
    colors_.text_disabled = lv_color_hex(0x9E9E9E);
    colors_.border = lv_color_hex(0xE0E0E0);
}

void Theme::loadDarkTheme() {
    colors_.primary = lv_color_hex(0x8AB4F8);
    colors_.primary_variant = lv_color_hex(0x4285F4);
    colors_.secondary = lv_color_hex(0x1E1E1E);
    colors_.background = lv_color_hex(0x121212);
    colors_.surface = lv_color_hex(0x1E1E1E);
    colors_.error = lv_color_hex(0xF28B82);
    colors_.success = lv_color_hex(0x81C995);
    colors_.warning = lv_color_hex(0xFFAB40);
    colors_.text_primary = lv_color_hex(0xFFFFFF);
    colors_.text_secondary = lv_color_hex(0xB0B0B0);
    colors_.text_disabled = lv_color_hex(0x616161);
    colors_.border = lv_color_hex(0x333333);
}

void Theme::apply() {
    // Apply theme to root screen
    lv_obj_t* root = lv_scr_act();
    lv_obj_set_style_bg_color(root, colors_.background, 0);
    
    // Reapply all widget styles
    // This would iterate through all screens and reapply styles
}

void Theme::setType(Type type) {
    if (type_ != type) {
        type_ = type;
        if (type == LIGHT) {
            loadLightTheme();
        } else {
            loadDarkTheme();
        }
        apply();
    }
}

Theme::Type Theme::getType() const {
    return type_;
}

const Theme::Colors& Theme::getColors() const {
    return colors_;
}
```

---

## Theme Manager

### Global Theme Management

```cpp
// theme_manager.hpp
#pragma once
#include "theme.hpp"

class ThemeManager {
public:
    static ThemeManager& getInstance();
    
    void initialize(Theme::Type type = Theme::LIGHT);
    Theme& getTheme();
    void setTheme(Theme::Type type);
    Theme::Type getCurrentTheme() const;
    void toggleTheme();
    
private:
    ThemeManager();
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    
    Theme theme_;
};

// theme_manager.cpp
#include "theme_manager.hpp"

ThemeManager& ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() : theme_(Theme::LIGHT) {
}

void ThemeManager::initialize(Theme::Type type) {
    theme_.setType(type);
    theme_.apply();
}

Theme& ThemeManager::getTheme() {
    return theme_;
}

void ThemeManager::setTheme(Theme::Type type) {
    theme_.setType(type);
}

Theme::Type ThemeManager::getCurrentTheme() const {
    return theme_.getType();
}

void ThemeManager::toggleTheme() {
    if (theme_.getType() == Theme::LIGHT) {
        setTheme(Theme::DARK);
    } else {
        setTheme(Theme::LIGHT);
    }
}
```

---

## Custom Themes

### Theme Configuration

Users can configure custom themes through settings:

```cpp
struct CustomThemeConfig {
    lv_color_t primary;
    lv_color_t secondary;
    lv_color_t background;
    lv_color_t text;
    bool useDarkBackground;
};

class ThemeSettings {
public:
    void loadCustomTheme();
    void saveCustomTheme(const CustomThemeConfig& config);
    CustomThemeConfig getCustomThemeConfig();
};
```

### Theme Editor UI

A settings screen section for theme customization:

**Color Pickers**:
- Primary color picker
- Secondary color picker
- Background color picker
- Text color picker

**Presets**:
- Light theme
- Dark theme
- High contrast
- Colorblind friendly
- Custom

**Preview**:
- Live preview of theme changes
- Reset to defaults

---

## Dynamic Theme Switching

### Auto Theme (Based on Time)

```cpp
class AutoThemeController {
public:
    AutoThemeController();
    
    void enable();
    void disable();
    void setSchedule(uint8_t sunriseHour, uint8_t sunsetHour);
    
private:
    bool enabled_;
    uint8_t sunriseHour_;
    uint8_t sunsetHour_;
    
    void checkAndUpdateTheme();
};

// In main loop or timer callback
void AutoThemeController::checkAndUpdateTheme() {
    if (!enabled_) return;
    
    auto timeData = DataStore::getInstance().getTimeData();
    uint8_t currentHour = timeData->hour;
    
    if (currentHour >= sunriseHour_ && currentHour < sunsetHour_) {
        ThemeManager::getInstance().setTheme(Theme::LIGHT);
    } else {
        ThemeManager::getInstance().setTheme(Theme::DARK);
    }
}
```

---

## Theme-Aware Widgets

### Creating Theme-Aware Widgets

All widgets should use the ThemeManager to get colors:

```cpp
class ThemedButton : public ButtonWidget {
public:
    ThemedButton(lv_obj_t* parent) : ButtonWidget(parent) {
        auto& theme = ThemeManager::getInstance().getTheme();
        setStyle(theme.getColors().primary, theme.getColors().text_primary);
    }
    
    void updateTheme() {
        auto& theme = ThemeManager::getInstance().getTheme();
        setStyle(theme.getColors().primary, theme.getColors().text_primary);
    }
};
```

### Theme Change Notification

```cpp
class ThemeListener {
public:
    virtual void onThemeChanged(Theme::Type newTheme) = 0;
};

class ThemeManager {
public:
    void addListener(ThemeListener* listener);
    void removeListener(ThemeListener* listener);
    
private:
    std::vector<ThemeListener*> listeners_;
    
    void notifyListeners(Theme::Type newTheme) {
        for (auto* listener : listeners_) {
            listener->onThemeChanged(newTheme);
        }
    }
};
```

---

## Theme Storage

### Persistence

Theme preferences are stored in flash memory:

```cpp
struct ThemePreferences {
    Theme::Type type;
    CustomThemeConfig customConfig;
    bool autoThemeEnabled;
    uint8_t sunriseHour;
    uint8_t sunsetHour;
};

class ThemeStorage {
public:
    void savePreferences(const ThemePreferences& prefs);
    ThemePreferences loadPreferences();
};
```

### Implementation with Preferences Library

```cpp
#include <Preferences.h>

Preferences preferences;

void ThemeStorage::savePreferences(const ThemePreferences& prefs) {
    preferences.begin("theme", false);
    preferences.putUInt("type", static_cast<uint8_t>(prefs.type));
    // Save other fields...
    preferences.end();
}

ThemePreferences ThemeStorage::loadPreferences() {
    ThemePreferences prefs;
    preferences.begin("theme", true);
    prefs.type = static_cast<Theme::Type>(preferences.getUInt("type", 0));
    // Load other fields...
    preferences.end();
    return prefs;
}
```

---

## Theme Best Practices

### Consistency

1. **Color Meaning**: Use consistent color meanings
   - Primary: Interactive elements
   - Success: Positive values, completed states
   - Error: Negative values, errors, warnings
   - Warning: Caution states

2. **Contrast**: Ensure sufficient contrast
   - Light theme: 4.5:1 minimum contrast ratio
   - Dark theme: 4.5:1 minimum contrast ratio
   - Test with contrast checker tools

3. **Accessibility**: Support colorblind users
   - Don't rely solely on color
   - Use additional visual cues (icons, text)
   - Support high contrast mode

### Performance

1. **Memory**: Themes are lightweight (just color values)
2. **Speed**: Theme switching should be instant
3. **Efficiency**: Only re-render changed elements

### Testing

1. **Visual Testing**: Verify all screens look good
2. **Contrast Testing**: Check text readability
3. **Accessibility Testing**: Test with colorblind simulators
4. **Hardware Testing**: Verify on actual display

---

## Summary

The theming system provides:

1. **Light and Dark Themes**: Complete color schemes
2. **Custom Themes**: User-configurable colors
3. **Auto Theme**: Time-based theme switching
4. **Theme Manager**: Centralized theme control
5. **Theme-Aware Widgets**: Automatic theme updates
6. **Theme Persistence**: Save preferences to flash

All UI components should use the ThemeManager to ensure consistent appearance and support theme switching.
