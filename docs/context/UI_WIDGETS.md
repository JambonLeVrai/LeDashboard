# UI Widgets Implementation

## Overview

This document provides detailed implementations of all UI widgets for the ESP32 dashboard using LVGL. Each widget is designed to be reusable, configurable, and efficient.

---

## Basic Widgets

### Label Widget

```cpp
// label_widget.hpp
#pragma once
#include <lvgl.h>

class LabelWidget {
public:
    LabelWidget(lv_obj_t* parent);
    
    void setText(const char* text);
    void setText(const std::string& text);
    void setFont(const lv_font_t* font);
    void setColor(lv_color_t color);
    void setAlign(lv_text_align_t align);
    void setLongMode(lv_label_long_mode_t mode);
    
    lv_obj_t* getObject() const;
    
private:
    lv_obj_t* label_{nullptr};
};

// label_widget.cpp
#include "label_widget.hpp"

LabelWidget::LabelWidget(lv_obj_t* parent) {
    label_ = lv_label_create(parent);
    lv_obj_set_style_text_font(label_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_, lv_color_hex(0x000000), 0);
    lv_label_set_long_mode(label_, LV_LABEL_LONG_WRAP);
}

void LabelWidget::setText(const char* text) {
    lv_label_set_text(label_, text);
}

void LabelWidget::setText(const std::string& text) {
    lv_label_set_text(label_, text.c_str());
}

void LabelWidget::setFont(const lv_font_t* font) {
    lv_obj_set_style_text_font(label_, font, 0);
}

void LabelWidget::setColor(lv_color_t color) {
    lv_obj_set_style_text_color(label_, color, 0);
}

void LabelWidget::setAlign(lv_text_align_t align) {
    lv_obj_set_style_text_align(label_, align, 0);
}

void LabelWidget::setLongMode(lv_label_long_mode_t mode) {
    lv_label_set_long_mode(label_, mode);
}

lv_obj_t* LabelWidget::getObject() const {
    return label_;
}
```

---

## Button Widget

```cpp
// button_widget.hpp
#pragma once
#include <lvgl.h>
#include <functional>

class ButtonWidget {
public:
    using ClickCallback = std::function<void()>;
    
    ButtonWidget(lv_obj_t* parent);
    
    void setText(const char* text);
    void setIcon(const void* img_src);
    void setSize(uint16_t width, uint16_t height);
    void setClickCallback(ClickCallback callback);
    void setStyle(lv_color_t bg_color, lv_color_t text_color);
    
    lv_obj_t* getObject() const;
    
private:
    lv_obj_t* btn_{nullptr};
    lv_obj_t* label_{nullptr};
    lv_obj_t* icon_{nullptr};
    ClickCallback click_callback_;
    
    static void clickHandler(lv_event_t* e);
};

// button_widget.cpp
#include "button_widget.hpp"

ButtonWidget::ButtonWidget(lv_obj_t* parent) {
    btn_ = lv_btn_create(parent);
    lv_obj_set_size(btn_, 120, 48);
    lv_obj_add_flag(btn_, LV_OBJ_FLAG_CLICKABLE);
    
    label_ = lv_label_create(btn_);
    lv_obj_center(label_);
    
    // Set default style
    lv_obj_set_style_bg_color(btn_, lv_color_hex(0x4285F4), 0);
    lv_obj_set_style_text_color(label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(btn_, 4, 0);
    lv_obj_set_style_border_width(btn_, 0, 0);
    
    // Add click handler
    lv_obj_add_event_cb(btn_, clickHandler, LV_EVENT_CLICKED, this);
}

void ButtonWidget::setText(const char* text) {
    lv_label_set_text(label_, text);
}

void ButtonWidget::setIcon(const void* img_src) {
    if (!icon_) {
        icon_ = lv_img_create(btn_);
    }
    lv_img_set_src(icon_, img_src);
    lv_obj_set_pos(icon_, 0, 0);
    
    if (label_) {
        lv_obj_set_x(label_, lv_img_get_width(icon_) + 8);
    }
}

void ButtonWidget::setSize(uint16_t width, uint16_t height) {
    lv_obj_set_size(btn_, width, height);
}

void ButtonWidget::setClickCallback(ClickCallback callback) {
    click_callback_ = callback;
}

void ButtonWidget::setStyle(lv_color_t bg_color, lv_color_t text_color) {
    lv_obj_set_style_bg_color(btn_, bg_color, 0);
    if (label_) {
        lv_obj_set_style_text_color(label_, text_color, 0);
    }
}

lv_obj_t* ButtonWidget::getObject() const {
    return btn_;
}

void ButtonWidget::clickHandler(lv_event_t* e) {
    ButtonWidget* widget = static_cast<ButtonWidget*>(lv_event_get_user_data(e));
    if (widget && widget->click_callback_) {
        widget->click_callback_();
    }
}
```

---

## Image Widget

```cpp
// image_widget.hpp
#pragma once
#include <lvgl.h>

class ImageWidget {
public:
    ImageWidget(lv_obj_t* parent);
    
    void setImage(const void* img_src);
    void setSize(uint16_t width, uint16_t height);
    void setAlign(lv_align_t align);
    
    lv_obj_t* getObject() const;
    
private:
    lv_obj_t* img_{nullptr};
};

// image_widget.cpp
#include "image_widget.hpp"

ImageWidget::ImageWidget(lv_obj_t* parent) {
    img_ = lv_img_create(parent);
}

void ImageWidget::setImage(const void* img_src) {
    lv_img_set_src(img_, img_src);
}

void ImageWidget::setSize(uint16_t width, uint16_t height) {
    lv_obj_set_size(img_, width, height);
}

void ImageWidget::setAlign(lv_align_t align) {
    lv_obj_set_align(img_, align);
}

lv_obj_t* ImageWidget::getObject() const {
    return img_;
}
```

---

## Progress Bar Widget

```cpp
// progress_bar_widget.hpp
#pragma once
#include <lvgl.h>

class ProgressBarWidget {
public:
    ProgressBarWidget(lv_obj_t* parent);
    
    void setValue(int value); // 0-100
    void setRange(int min, int max);
    void setColors(lv_color_t bg_color, lv_color_t indicator_color);
    void setSize(uint16_t width, uint16_t height);
    
    lv_obj_t* getObject() const;
    
private:
    lv_obj_t* bar_{nullptr};
};

// progress_bar_widget.cpp
#include "progress_bar_widget.hpp"

ProgressBarWidget::ProgressBarWidget(lv_obj_t* parent) {
    bar_ = lv_bar_create(parent);
    lv_obj_set_size(bar_, 200, 20);
    lv_bar_set_range(bar_, 0, 100);
    lv_bar_set_value(bar_, 0, LV_ANIM_OFF);
    
    // Style
    lv_obj_set_style_bg_color(bar_, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(0x4285F4), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_, 10, LV_PART_MAIN);
}

void ProgressBarWidget::setValue(int value) {
    lv_bar_set_value(bar_, value, LV_ANIM_OFF);
}

void ProgressBarWidget::setRange(int min, int max) {
    lv_bar_set_range(bar_, min, max);
}

void ProgressBarWidget::setColors(lv_color_t bg_color, lv_color_t indicator_color) {
    lv_obj_set_style_bg_color(bar_, bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_, indicator_color, LV_PART_INDICATOR);
}

void ProgressBarWidget::setSize(uint16_t width, uint16_t height) {
    lv_obj_set_size(bar_, width, height);
}

lv_obj_t* ProgressBarWidget::getObject() const {
    return bar_;
}
```

---

## Gauge Widget (Arc)

```cpp
// gauge_widget.hpp
#pragma once
#include <lvgl.h>
#include <string>

class GaugeWidget {
public:
    GaugeWidget(lv_obj_t* parent);
    
    void setValue(int value); // 0-100
    void setLabel(const char* label);
    void setColors(lv_color_t bg_color, lv_color_t needle_color, lv_color_t arc_color);
    void setSize(uint16_t width, uint16_t height);
    
    lv_obj_t* getObject() const;
    
private:
    lv_obj_t* cont_{nullptr};
    lv_obj_t* arc_{nullptr};
    lv_obj_t* label_{nullptr};
};

// gauge_widget.cpp
#include "gauge_widget.hpp"

GaugeWidget::GaugeWidget(lv_obj_t* parent) {
    cont_ = lv_obj_create(parent);
    lv_obj_set_size(cont_, 100, 100);
    lv_obj_set_layout(cont_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    arc_ = lv_arc_create(cont_);
    lv_obj_set_size(arc_, 80, 80);
    lv_arc_set_angles(arc_, 0, 360);
    lv_arc_set_bg_angles(arc_, 0, 360);
    lv_arc_set_value(arc_, 0);
    
    // Style
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0x4285F4), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, 2, LV_PART_MAIN);
    
    label_ = lv_label_create(cont_);
    lv_label_set_text(label_, "0%");
    lv_obj_set_style_text_font(label_, &lv_font_montserrat_16, 0);
}

void GaugeWidget::setValue(int value) {
    lv_arc_set_value(arc_, value);
    char label[16];
    snprintf(label, sizeof(label), "%d%%", value);
    lv_label_set_text(label_, label);
}

void GaugeWidget::setLabel(const char* label) {
    lv_label_set_text(label_, label);
}

void GaugeWidget::setColors(lv_color_t bg_color, lv_color_t needle_color, lv_color_t arc_color) {
    lv_obj_set_style_arc_color(arc_, bg_color, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_, arc_color, LV_PART_INDICATOR);
}

void GaugeWidget::setSize(uint16_t width, uint16_t height) {
    lv_obj_set_size(cont_, width, height);
}

lv_obj_t* GaugeWidget::getObject() const {
    return cont_;
}
```

---

## Chart Widget

```cpp
// chart_widget.hpp
#pragma once
#include <lvgl.h>
#include <vector>

class ChartWidget {
public:
    ChartWidget(lv_obj_t* parent);
    
    void setType(lv_chart_type_t type);
    void addSeries(lv_color_t color, const std::vector<int>& values);
    void clearSeries();
    void setRange(int min, int max);
    void setSize(uint16_t width, uint16_t height);
    
    lv_obj_t* getObject() const;
    
private:
    lv_obj_t* chart_{nullptr};
};

// chart_widget.cpp
#include "chart_widget.hpp"

ChartWidget::ChartWidget(lv_obj_t* parent) {
    chart_ = lv_chart_create(parent);
    lv_obj_set_size(chart_, 200, 100);
    lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
    
    // Configure axes
    lv_chart_set_axis_tick(chart_, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 10, 2, true, 40);
    lv_chart_set_axis_tick(chart_, LV_CHART_AXIS_PRIMARY_X, 10, 5, 10, 1, true, 30);
    lv_chart_set_div_line_count(chart_, 5, 5);
    
    // Style
    lv_obj_set_style_bg_color(chart_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(chart_, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_border_width(chart_, 1, 0);
}

void ChartWidget::setType(lv_chart_type_t type) {
    lv_chart_set_type(chart_, type);
}

void ChartWidget::addSeries(lv_color_t color, const std::vector<int>& values) {
    lv_chart_series_t* series = lv_chart_add_series(chart_, color, LV_CHART_AXIS_PRIMARY_Y);
    
    for (int value : values) {
        lv_chart_set_next_value(chart_, series, value);
    }
}

void ChartWidget::clearSeries() {
    lv_chart_clear_series(chart_);
}

void ChartWidget::setRange(int min, int max) {
    lv_chart_set_range(chart_, LV_CHART_AXIS_PRIMARY_Y, min, max);
}

void ChartWidget::setSize(uint16_t width, uint16_t height) {
    lv_obj_set_size(chart_, width, height);
}

lv_obj_t* ChartWidget::getObject() const {
    return chart_;
}
```

---

## Custom Widgets

### Time Widget

```cpp
// time_widget.hpp
#pragma once
#include "label_widget.hpp"
#include "data/data_store.hpp"

class TimeWidget {
public:
    TimeWidget(lv_obj_t* parent, DataStore& dataStore);
    void update();
    
private:
    lv_obj_t* cont_{nullptr};
    DataStore& dataStore_;
    
    LabelWidget* timeLabel_{nullptr};
    LabelWidget* dateLabel_{nullptr};
    LabelWidget* dayLabel_{nullptr};
};

// time_widget.cpp
#include "time_widget.hpp"
#include <ctime>

TimeWidget::TimeWidget(lv_obj_t* parent, DataStore& dataStore)
    : dataStore_(dataStore) {
    
    cont_ = lv_obj_create(parent);
    lv_obj_set_layout(cont_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont_, 0, 0);
    lv_obj_set_style_pad_gap(cont_, 4, 0);
    lv_obj_set_size(cont_, 240, 150);
    
    // Time label (64pt)
    timeLabel_ = new LabelWidget(cont_);
    timeLabel_->setFont(&lv_font_montserrat_64);
    timeLabel_->setText("12:34");
    
    // Date label (20pt)
    dateLabel_ = new LabelWidget(cont_);
    dateLabel_->setFont(&lv_font_montserrat_20);
    dateLabel_->setText("October 1");
    
    // Day label (16pt)
    dayLabel_ = new LabelWidget(cont_);
    dayLabel_->setFont(&lv_font_montserrat_16);
    dayLabel_->setColor(lv_color_hex(0x616161));
    dayLabel_->setText("Monday");
}

void TimeWidget::update() {
    auto timeData = dataStore_.getTimeData();
    
    if (timeData) {
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                timeData->hour, timeData->minute, timeData->second);
        timeLabel_->setText(timeStr);
        
        char dateStr[32];
        snprintf(dateStr, sizeof(dateStr), "%s %d", 
                getMonthName(timeData->month), timeData->day);
        dateLabel_->setText(dateStr);
        
        dayLabel_->setText(getDayName(timeData->day_of_week));
    }
}
```

---

### Weather Widget

```cpp
// weather_widget.hpp
#pragma once
#include "label_widget.hpp"
#include "image_widget.hpp"
#include "data/data_store.hpp"

class WeatherWidget {
public:
    WeatherWidget(lv_obj_t* parent, DataStore& dataStore);
    void update();
    
private:
    lv_obj_t* cont_{nullptr};
    DataStore& dataStore_;
    
    ImageWidget* icon_{nullptr};
    LabelWidget* tempLabel_{nullptr};
    LabelWidget* condLabel_{nullptr};
    lv_obj_t* detailsCont_{nullptr};
    
    const void* getWeatherIcon(const std::string& conditionCode);
};

// weather_widget.cpp
#include "weather_widget.hpp"

// Weather icon mappings
LV_IMG_DECLARE(icon_weather_sun);
LV_IMG_DECLARE(icon_weather_cloud);
LV_IMG_DECLARE(icon_weather_rain);
LV_IMG_DECLARE(icon_weather_snow);
LV_IMG_DECLARE(icon_weather_thunder);
// ... more icons

WeatherWidget::WeatherWidget(lv_obj_t* parent, DataStore& dataStore)
    : dataStore_(dataStore) {
    
    cont_ = lv_obj_create(parent);
    lv_obj_set_layout(cont_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont_, 0, 0);
    lv_obj_set_style_pad_gap(cont_, 8, 0);
    lv_obj_set_size(cont_, 240, 150);
    
    // Icon
    lv_obj_t* iconCont = lv_obj_create(cont_);
    lv_obj_set_layout(iconCont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(iconCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(iconCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    icon_ = new ImageWidget(iconCont);
    icon_->setSize(64, 64);
    
    // Text container
    lv_obj_t* textCont = lv_obj_create(cont_);
    lv_obj_set_layout(textCont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(textCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(textCont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(textCont, 1);
    
    // Temperature label
    tempLabel_ = new LabelWidget(textCont);
    tempLabel_->setFont(&lv_font_montserrat_64);
    tempLabel_->setText("22");
    
    // Condition label
    condLabel_ = new LabelWidget(textCont);
    condLabel_->setFont(&lv_font_montserrat_20);
    condLabel_->setText("Sunny");
    
    // Details container
    detailsCont_ = lv_obj_create(cont_);
    lv_obj_set_layout(detailsCont_, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(detailsCont_, grid_cols, grid_rows);
    lv_obj_set_size(detailsCont_, 200, 40);
}

const void* WeatherWidget::getWeatherIcon(const std::string& conditionCode) {
    if (conditionCode == "01d" || conditionCode == "01n") return &icon_weather_sun;
    if (conditionCode == "02d" || conditionCode == "02n") return &icon_weather_cloud;
    if (conditionCode == "03d" || conditionCode == "03n") return &icon_weather_cloud;
    if (conditionCode == "04d" || conditionCode == "04n") return &icon_weather_cloud;
    if (conditionCode.find("09") == 0) return &icon_weather_rain;
    if (conditionCode.find("10") == 0) return &icon_weather_rain;
    if (conditionCode.find("11") == 0) return &icon_weather_thunder;
    if (conditionCode.find("13") == 0) return &icon_weather_snow;
    return &icon_weather_sun; // Default
}

void WeatherWidget::update() {
    auto weatherData = dataStore_.getWeatherData();
    
    if (weatherData) {
        // Temperature
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%.0f", weatherData->temperature);
        tempLabel_->setText(tempStr);
        
        // Condition
        condLabel_->setText(weatherData->condition.c_str());
        
        // Icon
        icon_->setImage(getWeatherIcon(weatherData->conditionCode));
        
        // Update details
        // ...
    }
}
```

---

### Media Widget

```cpp
// media_widget.hpp
#pragma once
#include "label_widget.hpp"
#include "image_widget.hpp"
#include "progress_bar_widget.hpp"
#include "button_widget.hpp"
#include "data/data_store.hpp"

class MediaWidget {
public:
    MediaWidget(lv_obj_t* parent, DataStore& dataStore);
    void update();
    
private:
    lv_obj_t* cont_{nullptr};
    DataStore& dataStore_;
    
    ImageWidget* albumArt_{nullptr};
    LabelWidget* titleLabel_{nullptr};
    LabelWidget* artistLabel_{nullptr};
    ProgressBarWidget* progressBar_{nullptr};
    LabelWidget* timeLabel_{nullptr};
    ButtonWidget* playBtn_{nullptr};
};

// media_widget.cpp
#include "media_widget.hpp"

LV_IMG_DECLARE(default_album_art);
LV_IMG_DECLARE(icon_play);
LV_IMG_DECLARE(icon_pause);

MediaWidget::MediaWidget(lv_obj_t* parent, DataStore& dataStore)
    : dataStore_(dataStore) {
    
    cont_ = lv_obj_create(parent);
    lv_obj_set_layout(cont_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(cont_, 0, 0);
    lv_obj_set_style_pad_gap(cont_, 4, 0);
    lv_obj_set_size(cont_, 240, 150);
    
    // Album art
    albumArt_ = new ImageWidget(cont_);
    albumArt_->setSize(120, 120);
    albumArt_->setImage(&default_album_art);
    
    // Text container
    lv_obj_t* textCont = lv_obj_create(cont_);
    lv_obj_set_layout(textCont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(textCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(textCont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_flex_grow(textCont, 1);
    lv_obj_set_style_pad_left(textCont, 8, 0);
    
    // Title
    titleLabel_ = new LabelWidget(textCont);
    titleLabel_->setFont(&lv_font_montserrat_20);
    titleLabel_->setText("Song Title");
    titleLabel_->setLongMode(LV_LABEL_LONG_DOT);
    lv_obj_set_width(titleLabel_->getObject(), 240 - 128);
    
    // Artist
    artistLabel_ = new LabelWidget(textCont);
    artistLabel_->setFont(&lv_font_montserrat_16);
    artistLabel_->setColor(lv_color_hex(0x616161));
    artistLabel_->setText("Artist Name");
    artistLabel_->setLongMode(LV_LABEL_LONG_DOT);
    lv_obj_set_width(artistLabel_->getObject(), 240 - 128);
    
    // Progress bar
    lv_obj_t* progressCont = lv_obj_create(cont_);
    lv_obj_set_layout(progressCont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(progressCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(progressCont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(progressCont, 0, 0);
    lv_obj_set_width(progressCont, 240);
    
    progressBar_ = new ProgressBarWidget(progressCont);
    progressBar_->setSize(240 - 80, 8);
    
    timeLabel_ = new LabelWidget(progressCont);
    timeLabel_->setFont(&lv_font_montserrat_14);
    timeLabel_->setText("0:00 / 0:00");
    
    // Play button
    lv_obj_t* btnCont = lv_obj_create(cont_);
    lv_obj_set_layout(btnCont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    playBtn_ = new ButtonWidget(btnCont);
    playBtn_->setSize(48, 48);
    playBtn_->setIcon(&icon_play);
    playBtn_->setClickCallback([this]() {
        // Send play/pause command to server
    });
}

void MediaWidget::update() {
    auto mediaData = dataStore_.getMediaData();
    
    if (mediaData) {
        // Title
        titleLabel_->setText(mediaData->title.c_str());
        
        // Artist
        artistLabel_->setText(mediaData->artist.c_str());
        
        // Progress
        int progress = static_cast<int>((mediaData->position * 100.0) / mediaData->duration);
        progressBar_->setValue(progress);
        
        // Time
        char timeStr[32];
        int mins = mediaData->position / 60;
        int secs = mediaData->position % 60;
        int totalMins = mediaData->duration / 60;
        int totalSecs = mediaData->duration % 60;
        snprintf(timeStr, sizeof(timeStr), "%d:%02d / %d:%02d", 
                mins, secs, totalMins, totalSecs);
        timeLabel_->setText(timeStr);
        
        // Play button icon
        if (mediaData->isPlaying) {
            playBtn_->setIcon(&icon_pause);
        } else {
            playBtn_->setIcon(&icon_play);
        }
    }
}
```

---

### Notification Widget

```cpp
// notification_widget.hpp
#pragma once
#include "label_widget.hpp"
#include "image_widget.hpp"
#include "data/data_store.hpp"

class NotificationWidget {
public:
    NotificationWidget(lv_obj_t* parent, DataStore& dataStore);
    void update();
    void setNotification(const NotificationData& notification);
    
private:
    lv_obj_t* cont_{nullptr};
    DataStore& dataStore_;
    
    lv_obj_t* badge_{nullptr};
    LabelWidget* badgeLabel_{nullptr};
    ImageWidget* icon_{nullptr};
    LabelWidget* titleLabel_{nullptr};
    LabelWidget* senderLabel_{nullptr};
    LabelWidget* previewLabel_{nullptr};
    LabelWidget* timeLabel_{nullptr};
    
    const void* getNotificationIcon(NotificationType type);
};

// notification_widget.cpp
#include "notification_widget.hpp"

LV_IMG_DECLARE(icon_email);
LV_IMG_DECLARE(icon_app);
LV_IMG_DECLARE(icon_system);
LV_IMG_DECLARE(icon_calendar);

NotificationWidget::NotificationWidget(lv_obj_t* parent, DataStore& dataStore)
    : dataStore_(dataStore) {
    
    cont_ = lv_obj_create(parent);
    lv_obj_set_size(cont_, 240, 150);
    lv_obj_set_style_bg_color(cont_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(cont_, 8, 0);
    lv_obj_set_style_border_width(cont_, 1, 0);
    lv_obj_set_style_border_color(cont_, lv_color_hex(0xE0E0E0), 0);
    
    // Badge
    badge_ = lv_obj_create(cont_);
    lv_obj_set_pos(badge_, 8, 8);
    lv_obj_set_size(badge_, 32, 32);
    lv_obj_set_style_bg_color(badge_, lv_color_hex(0x4285F4), 0);
    lv_obj_set_style_radius(badge_, 16, 0);
    
    badgeLabel_ = new LabelWidget(badge_);
    badgeLabel_->setFont(&lv_font_montserrat_16);
    badgeLabel_->setColor(lv_color_hex(0xFFFFFF));
    badgeLabel_->setText("3");
    
    // Icon
    icon_ = new ImageWidget(cont_);
    icon_->setSize(24, 24);
    lv_obj_set_pos(icon_->getObject(), 200, 8);
    
    // Time
    timeLabel_ = new LabelWidget(cont_);
    timeLabel_->setFont(&lv_font_montserrat_12);
    timeLabel_->setColor(lv_color_hex(0x616161));
    lv_obj_set_pos(timeLabel_->getObject(), 160, 8);
    
    // Title
    titleLabel_ = new LabelWidget(cont_);
    titleLabel_->setFont(&lv_font_montserrat_16);
    lv_obj_set_pos(titleLabel_->getObject(), 48, 8);
    lv_obj_set_width(titleLabel_->getObject(), 180);
    
    // Sender
    senderLabel_ = new LabelWidget(cont_);
    senderLabel_->setFont(&lv_font_montserrat_14);
    senderLabel_->setColor(lv_color_hex(0x616161));
    lv_obj_set_pos(senderLabel_->getObject(), 48, 30);
    lv_obj_set_width(senderLabel_->getObject(), 180);
    
    // Preview
    previewLabel_ = new LabelWidget(cont_);
    previewLabel_->setFont(&lv_font_montserrat_14);
    previewLabel_->setColor(lv_color_hex(0x616161));
    lv_obj_set_pos(previewLabel_->getObject(), 8, 60);
    lv_obj_set_width(previewLabel_->getObject(), 224);
    previewLabel_->setLongMode(LV_LABEL_LONG_DOT);
}

const void* NotificationWidget::getNotificationIcon(NotificationType type) {
    switch (type) {
        case NOTIFICATION_EMAIL: return &icon_email;
        case NOTIFICATION_APP: return &icon_app;
        case NOTIFICATION_SYSTEM: return &icon_system;
        case NOTIFICATION_CALENDAR: return &icon_calendar;
        default: return &icon_system;
    }
}

void NotificationWidget::setNotification(const NotificationData& notification) {
    // Badge
    char badgeStr[8];
    snprintf(badgeStr, sizeof(badgeStr), "%d", notification.unreadCount);
    badgeLabel_->setText(badgeStr);
    
    // Icon
    icon_->setImage(getNotificationIcon(notification.type));
    
    // Time
    char timeStr[16];
    strftime(timeStr, sizeof(timeStr), "%H:%M", localtime(&notification.timestamp));
    timeLabel_->setText(timeStr);
    
    // Title
    titleLabel_->setText(notification.title.c_str());
    
    // Sender
    senderLabel_->setText(notification.sender.c_str());
    
    // Preview
    previewLabel_->setText(notification.preview.c_str());
}

void NotificationWidget::update() {
    auto notificationData = dataStore_.getLatestNotification();
    if (notificationData) {
        setNotification(*notificationData);
    }
}
```

---

### Portfolio Widget

```cpp
// portfolio_widget.hpp
#pragma once
#include "label_widget.hpp"
#include "chart_widget.hpp"
#include "data/data_store.hpp"

class PortfolioWidget {
public:
    PortfolioWidget(lv_obj_t* parent, DataStore& dataStore);
    void update();
    
private:
    lv_obj_t* cont_{nullptr};
    DataStore& dataStore_;
    
    LabelWidget* totalLabel_{nullptr};
    LabelWidget* changeLabel_{nullptr};
    LabelWidget* changePctLabel_{nullptr};
    ChartWidget* chart_{nullptr};
};

// portfolio_widget.cpp
#include "portfolio_widget.hpp"

PortfolioWidget::PortfolioWidget(lv_obj_t* parent, DataStore& dataStore)
    : dataStore_(dataStore) {
    
    cont_ = lv_obj_create(parent);
    lv_obj_set_layout(cont_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont_, 0, 0);
    lv_obj_set_style_pad_gap(cont_, 4, 0);
    lv_obj_set_size(cont_, 240, 150);
    
    // Total value
    totalLabel_ = new LabelWidget(cont_);
    totalLabel_->setFont(&lv_font_montserrat_48);
    totalLabel_->setText("$125,432");
    
    // Change
    lv_obj_t* changeCont = lv_obj_create(cont_);
    lv_obj_set_layout(changeCont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(changeCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(changeCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(changeCont, 0, 0);
    lv_obj_set_style_pad_gap(changeCont, 8, 0);
    
    changeLabel_ = new LabelWidget(changeCont);
    changeLabel_->setFont(&lv_font_montserrat_24);
    changeLabel_->setText("+$456");
    
    changePctLabel_ = new LabelWidget(changeCont);
    changePctLabel_->setFont(&lv_font_montserrat_20);
    changePctLabel_->setText("+0.36%");
    changePctLabel_->setColor(lv_color_hex(0x0F9D58)); // Green
    
    // Chart
    chart_ = new ChartWidget(cont_);
    chart_->setSize(200, 50);
    chart_->setType(LV_CHART_TYPE_LINE);
    
    // Add sample data
    std::vector<int> values = {100, 105, 110, 108, 115, 120, 118, 125};
    chart_->addSeries(lv_color_hex(0x4285F4), values);
}

void PortfolioWidget::update() {
    auto portfolioData = dataStore_.getPortfolioData();
    
    if (portfolioData) {
        // Total value
        char totalStr[32];
        snprintf(totalStr, sizeof(totalStr), "$%.2f", portfolioData->totalValue);
        totalLabel_->setText(totalStr);
        
        // Change
        char changeStr[32];
        if (portfolioData->todayChange >= 0) {
            snprintf(changeStr, sizeof(changeStr), "+$%.2f", portfolioData->todayChange);
            changeLabel_->setColor(lv_color_hex(0x0F9D58)); // Green
        } else {
            snprintf(changeStr, sizeof(changeStr), "-$%.2f", -portfolioData->todayChange);
            changeLabel_->setColor(lv_color_hex(0xDB4437)); // Red
        }
        changeLabel_->setText(changeStr);
        
        // Change percentage
        char pctStr[16];
        snprintf(pctStr, sizeof(pctStr), "%.2f%%", portfolioData->todayChangePct);
        changePctLabel_->setText(pctStr);
        changePctLabel_->setColor(changeLabel_->getObject()->style_text.color);
        
        // Update chart
        // ...
    }
}
```

---

### System Widget

```cpp
// system_widget.hpp
#pragma once
#include "gauge_widget.hpp"
#include "label_widget.hpp"
#include "data/data_store.hpp"

class SystemWidget {
public:
    SystemWidget(lv_obj_t* parent, DataStore& dataStore);
    void update();
    
private:
    lv_obj_t* cont_{nullptr};
    DataStore& dataStore_;
    
    GaugeWidget* cpuGauge_{nullptr};
    GaugeWidget* memGauge_{nullptr};
    GaugeWidget* diskGauge_{nullptr};
    LabelWidget* networkLabel_{nullptr};
};

// system_widget.cpp
#include "system_widget.hpp"

SystemWidget::SystemWidget(lv_obj_t* parent, DataStore& dataStore)
    : dataStore_(dataStore) {
    
    cont_ = lv_obj_create(parent);
    lv_obj_set_layout(cont_, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(cont_, 0, 0);
    lv_obj_set_style_pad_gap(cont_, 8, 0);
    lv_obj_set_size(cont_, 240, 150);
    
    // CPU Gauge
    cpuGauge_ = new GaugeWidget(cont_);
    cpuGauge_->setSize(80, 80);
    cpuGauge_->setLabel("CPU");
    cpuGauge_->setValue(25);
    
    // Memory Gauge
    memGauge_ = new GaugeWidget(cont_);
    memGauge_->setSize(80, 80);
    memGauge_->setLabel("MEM");
    memGauge_->setValue(60);
    
    // Disk Gauge
    diskGauge_ = new GaugeWidget(cont_);
    diskGauge_->setSize(80, 80);
    diskGauge_->setLabel("DISK");
    diskGauge_->setValue(45);
    
    // Network label
    networkLabel_ = new LabelWidget(cont_);
    networkLabel_->setFont(&lv_font_montserrat_14);
    networkLabel_->setText("Net: ↑1.2/↓5.6");
}

void SystemWidget::update() {
    auto systemData = dataStore_.getSystemData();
    
    if (systemData) {
        cpuGauge_->setValue(static_cast<int>(systemData->cpuUsage));
        memGauge_->setValue(static_cast<int>(systemData->memoryUsage));
        diskGauge_->setValue(static_cast<int>(systemData->diskUsage));
        
        char netStr[32];
        snprintf(netStr, sizeof(netStr), "Net: ↑%.1f/↓%.1f", 
                systemData->netUp, systemData->netDown);
        networkLabel_->setText(netStr);
    }
}
```

---

## Container Widgets

### Card Widget

```cpp
// card_widget.hpp
#pragma once
#include <lvgl.h>
#include <functional>

class CardWidget {
public:
    using ClickCallback = std::function<void()>;
    
    CardWidget(lv_obj_t* parent);
    
    void setTitle(const char* title);
    void setContent(lv_obj_t* content);
    void setClickCallback(ClickCallback callback);
    void setSize(uint16_t width, uint16_t height);
    
    lv_obj_t* getObject() const;
    lv_obj_t* getContentArea() const;
    
private:
    lv_obj_t* card_{nullptr};
    lv_obj_t* titleLabel_{nullptr};
    lv_obj_t* contentArea_{nullptr};
    ClickCallback click_callback_;
    
    static void clickHandler(lv_event_t* e);
};

// card_widget.cpp
#include "card_widget.hpp"

CardWidget::CardWidget(lv_obj_t* parent) {
    card_ = lv_obj_create(parent);
    lv_obj_set_layout(card_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(card_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(card_, 8, 0);
    lv_obj_set_style_border_width(card_, 1, 0);
    lv_obj_set_style_border_color(card_, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_pad_all(card_, 8, 0);
    lv_obj_set_style_pad_gap(card_, 4, 0);
    lv_obj_add_flag(card_, LV_OBJ_FLAG_CLICKABLE);
    
    // Title
    titleLabel_ = lv_label_create(card_);
    lv_obj_set_style_text_font(titleLabel_, &lv_font_montserrat_20, 0);
    
    // Content area
    contentArea_ = lv_obj_create(card_);
    lv_obj_set_layout(contentArea_, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(contentArea_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(contentArea_, 1);
    
    // Add click handler
    lv_obj_add_event_cb(card_, clickHandler, LV_EVENT_CLICKED, this);
}

void CardWidget::setTitle(const char* title) {
    lv_label_set_text(titleLabel_, title);
}

void CardWidget::setContent(lv_obj_t* content) {
    if (contentArea_) {
        lv_obj_clean(contentArea_);
        lv_obj_add(contentArea_, content);
    }
}

void CardWidget::setClickCallback(ClickCallback callback) {
    click_callback_ = callback;
}

void CardWidget::setSize(uint16_t width, uint16_t height) {
    lv_obj_set_size(card_, width, height);
}

lv_obj_t* CardWidget::getObject() const {
    return card_;
}

lv_obj_t* CardWidget::getContentArea() const {
    return contentArea_;
}

void CardWidget::clickHandler(lv_event_t* e) {
    CardWidget* widget = static_cast<CardWidget*>(lv_event_get_user_data(e));
    if (widget && widget->click_callback_) {
        widget->click_callback_();
    }
}
```

---

### Badge Widget

```cpp
// badge_widget.hpp
#pragma once
#include "label_widget.hpp"

class BadgeWidget {
public:
    BadgeWidget(lv_obj_t* parent);
    
    void setValue(int value);
    void setColor(lv_color_t color);
    void setSize(uint16_t size);
    
    lv_obj_t* getObject() const;
    
private:
    lv_obj_t* badge_{nullptr};
    LabelWidget* label_{nullptr};
};

// badge_widget.cpp
#include "badge_widget.hpp"

BadgeWidget::BadgeWidget(lv_obj_t* parent) {
    badge_ = lv_obj_create(parent);
    lv_obj_set_size(badge_, 32, 32);
    lv_obj_set_style_bg_color(badge_, lv_color_hex(0xDB4437), 0); // Red by default
    lv_obj_set_style_radius(badge_, 16, 0);
    
    label_ = new LabelWidget(badge_);
    label_->setFont(&lv_font_montserrat_16);
    label_->setColor(lv_color_hex(0xFFFFFF));
    label_->setText("0");
}

void BadgeWidget::setValue(int value) {
    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%d", value);
    label_->setText(valStr);
}

void BadgeWidget::setColor(lv_color_t color) {
    lv_obj_set_style_bg_color(badge_, color, 0);
}

void BadgeWidget::setSize(uint16_t size) {
    lv_obj_set_size(badge_, size, size);
    lv_obj_set_style_radius(badge_, size / 2, 0);
}

lv_obj_t* BadgeWidget::getObject() const {
    return badge_;
}
```

---

## Utility Widgets

### Separator Widget

```cpp
// separator_widget.hpp
#pragma once
#include <lvgl.h>

class SeparatorWidget {
public:
    SeparatorWidget(lv_obj_t* parent);
    
    void setHorizontal(bool horizontal);
    void setColor(lv_color_t color);
    void setThickness(uint8_t thickness);
    
private:
    lv_obj_t* separator_{nullptr};
};

// separator_widget.cpp
#include "separator_widget.hpp"

SeparatorWidget::SeparatorWidget(lv_obj_t* parent) {
    separator_ = lv_obj_create(parent);
    lv_obj_set_size(separator_, 1, 1);
    lv_obj_set_style_bg_color(separator_, lv_color_hex(0xE0E0E0), 0);
}

void SeparatorWidget::setHorizontal(bool horizontal) {
    if (horizontal) {
        lv_obj_set_width(separator_, 1);
        lv_obj_set_height(separator_, LV_SIZE_CONTENT);
    } else {
        lv_obj_set_width(separator_, LV_SIZE_CONTENT);
        lv_obj_set_height(separator_, 1);
    }
}

void SeparatorWidget::setColor(lv_color_t color) {
    lv_obj_set_style_bg_color(separator_, color, 0);
}

void SeparatorWidget::setThickness(uint8_t thickness) {
    if (lv_obj_get_width(separator_) > lv_obj_get_height(separator_)) {
        lv_obj_set_height(separator_, thickness);
    } else {
        lv_obj_set_width(separator_, thickness);
    }
}
```

---

## Summary

This document provides implementations for all UI widgets needed for the ESP32 dashboard. The widgets are designed to be:

1. **Reusable**: Can be used in multiple places
2. **Configurable**: Customizable appearance and behavior
3. **Efficient**: Minimal memory and CPU usage
4. **Consistent**: Unified look and feel
5. **Maintainable**: Clean, modular code

To use these widgets:
1. Include the appropriate header file
2. Create the widget with a parent object
3. Configure the widget (size, colors, etc.)
4. Add it to your screen layout
5. Update it when data changes

See the UI_DESIGN.md for overall UI structure and layout guidelines.
