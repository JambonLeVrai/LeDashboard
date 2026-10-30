# Animations Specification

## Overview

This document specifies the **animation system** for the ESP32 dashboard, providing smooth transitions and visual feedback for user interactions.

---

## Animation Architecture

### Animation System Components

```
┌─────────────────────────────────────────┐
│              LVGL Animation API             │
├─────────────────────────────────────────┤
│  lv_anim_t structure                        │
│  - var: animated object                    │
│  - start: start value                      │
│  - end: end value                          │
│  - time: duration in ms                     │
│  - exec_cb: animation function              │
│  - path_cb: easing function                 │
│  - ready_cb: completion callback           │
│  - act_time: actual elapsed time            │
│  - repeat_count: repetition count           │
└─────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│            Animation Wrapper                │
│  - ScreenTransitions                       │
│  - WidgetAnimations                        │
│  - CustomAnimations                        │
└─────────────────────────────────────────┘
```

### Animation Types

| Type | Description | LVGL Type |
|------|-------------|------------|
| **Property** | Animate object properties | `lv_anim_t` |
| **Style** | Animate style properties | `lv_anim_t` |
| **Path** | Animate along a path | Custom |
| **Sequence** | Chain multiple animations | Custom |

---

## Easing Functions

### Available Easing Functions

| Function | Description | LVGL Name | Use Case |
|----------|-------------|-----------|----------|
| Linear | Constant speed | `lv_anim_path_linear` | Simple animations |
| Ease In | Slow start, fast end | `lv_anim_path_ease_in` | Entering elements |
| Ease Out | Fast start, slow end | `lv_anim_path_ease_out` | Exiting elements |
| Ease In Out | Slow start and end | `lv_anim_path_ease_in_out` | Smooth transitions |
| Overshoot | Bounces slightly past | `lv_anim_path_overshoot` | Playful feedback |
| Bounce | Bounces multiple times | `lv_anim_path_bounce` | Attention-grabbing |
| Step | Instant jumps | `lv_anim_path_step` | Digital clocks |

### Custom Easing

Create custom easing functions for specific effects:

```cpp
int32_t custom_ease_in_out(const lv_anim_t * a) {
    int32_t t = a->act_time;
    int32_t d = a->time;
    
    if (t < d / 2) {
        return 2 * t * t / d;
    } else {
        t = d - t;
        return 2 * t * t / d;
    }
}
```

---

## Screen Transitions

### Slide Animations

```cpp
class ScreenTransitions {
public:
    enum Direction { LEFT, RIGHT, UP, DOWN };
    
    // Slide in from edge
    static void slideIn(lv_obj_t* obj, Direction dir, uint32_t time = 300);
    
    // Slide out to edge
    static void slideOut(lv_obj_t* obj, Direction dir, uint32_t time = 300);
};

// Implementation
void ScreenTransitions::slideIn(lv_obj_t* obj, Direction dir, uint32_t time) {
    // Get screen dimensions
    int16_t start_pos = 0;
    
    switch (dir) {
        case LEFT:  start_pos = LV_HOR_RES_MAX; break;  // Slide in from right
        case RIGHT: start_pos = -LV_HOR_RES_MAX; break; // Slide in from left
        case UP:    start_pos = LV_VER_RES_MAX; break;  // Slide in from bottom
        case DOWN:  start_pos = -LV_VER_RES_MAX; break; // Slide in from top
    }
    
    // Set initial position
    if (dir == LEFT || dir == RIGHT) {
        lv_obj_set_x(obj, start_pos);
    } else {
        lv_obj_set_y(obj, start_pos);
    }
    
    // Create animation
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_time(&anim, time);
    lv_anim_set_values(&anim, start_pos, 0);
    
    // Set animation callback based on direction
    if (dir == LEFT || dir == RIGHT) {
        lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    } else {
        lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    }
    
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void ScreenTransitions::slideOut(lv_obj_t* obj, Direction dir, uint32_t time) {
    int16_t end_pos = 0;
    
    switch (dir) {
        case LEFT:  end_pos = -LV_HOR_RES_MAX; break; // Slide out to left
        case RIGHT: end_pos = LV_HOR_RES_MAX; break;  // Slide out to right
        case UP:    end_pos = -LV_VER_RES_MAX; break; // Slide out to top
        case DOWN:  end_pos = LV_VER_RES_MAX; break;  // Slide out to bottom
    }
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_time(&anim, time);
    
    if (dir == LEFT || dir == RIGHT) {
        lv_anim_set_values(&anim, lv_obj_get_x(obj), end_pos);
        lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    } else {
        lv_anim_set_values(&anim, lv_obj_get_y(obj), end_pos);
        lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    }
    
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
        lv_obj_set_pos(lv_anim_get_var(a), 0, 0);
    });
    lv_anim_start(&anim);
}
```

### Fade Animations

```cpp
class ScreenTransitions {
public:
    // Fade in (appear)
    static void fadeIn(lv_obj_t* obj, uint32_t time = 200);
    
    // Fade out (disappear)
    static void fadeOut(lv_obj_t* obj, uint32_t time = 200);
};

void ScreenTransitions::fadeIn(lv_obj_t* obj, uint32_t time) {
    // Start invisible
    lv_obj_set_style_opa(obj, 0, 0);
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_time(&anim, time);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* obj = lv_anim_get_var(a);
        lv_obj_set_style_opa(obj, a->act_time * 255 / a->time, 0);
    });
    lv_anim_set_values(&anim, 0, 255);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_start(&anim);
}

void ScreenTransitions::fadeOut(lv_obj_t* obj, uint32_t time) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_time(&anim, time);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* obj = lv_anim_get_var(a);
        lv_obj_set_style_opa(obj, 255 - (a->act_time * 255 / a->time), 0);
    });
    lv_anim_set_values(&anim, 255, 0);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
        lv_obj_set_style_opa(lv_anim_get_var(a), 255, 0);
    });
    lv_anim_start(&anim);
}
```

### Scale Animations

```cpp
class ScreenTransitions {
public:
    // Scale in (zoom in)
    static void scaleIn(lv_obj_t* obj, uint32_t time = 200);
    
    // Scale out (zoom out)
    static void scaleOut(lv_obj_t* obj, uint32_t time = 200);
};

void ScreenTransitions::scaleIn(lv_obj_t* obj, uint32_t time) {
    // Start small
    lv_obj_set_size(obj, lv_obj_get_width(obj) / 2, lv_obj_get_height(obj) / 2);
    lv_obj_center(obj);
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_time(&anim, time);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* obj = lv_anim_get_var(a);
        int32_t w = lv_obj_get_width(obj) * a->time / a->act_time;
        int32_t h = lv_obj_get_height(obj) * a->time / a->act_time;
        lv_obj_set_size(obj, w, h);
        lv_obj_center(obj);
    });
    lv_anim_set_values(&anim, 0, 100);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void ScreenTransitions::scaleOut(lv_obj_t* obj, uint32_t time) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_time(&anim, time);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* obj = lv_anim_get_var(a);
        int32_t w = lv_obj_get_width(obj) * a->act_time / a->time;
        int32_t h = lv_obj_get_height(obj) * a->act_time / a->time;
        lv_obj_set_size(obj, w, h);
        lv_obj_center(obj);
    });
    lv_anim_set_values(&anim, 100, 0);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
        lv_obj_set_size(lv_anim_get_var(a), LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    });
    lv_anim_start(&anim);
}
```

---

## Widget Animations

### Button Press Animation

```cpp
class ButtonAnimations {
public:
    // Press animation (scale down slightly)
    static void press(lv_obj_t* btn);
    
    // Release animation (scale back)
    static void release(lv_obj_t* btn);
};

void ButtonAnimations::press(lv_obj_t* btn) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, btn);
    lv_anim_set_time(&anim, 100);
    lv_anim_set_values(&anim, 100, 90);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* btn = lv_anim_get_var(a);
        int16_t w = lv_obj_get_width(btn) * a->act_time / a->time;
        int16_t h = lv_obj_get_height(btn) * a->act_time / a->time;
        lv_obj_set_size(btn, w, h);
    });
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void ButtonAnimations::release(lv_obj_t* btn) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, btn);
    lv_anim_set_time(&anim, 150);
    lv_anim_set_values(&anim, 90, 100);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* btn = lv_anim_get_var(a);
        int16_t w = lv_obj_get_width(btn) * a->time / a->act_time;
        int16_t h = lv_obj_get_height(btn) * a->time / a->act_time;
        lv_obj_set_size(btn, w, h);
    });
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}
```

### Progress Bar Animation

```cpp
class ProgressBarAnimations {
public:
    // Animate to new value
    static void animateTo(lv_obj_t* bar, int value, uint32_t time = 500);
};

void ProgressBarAnimations::animateTo(lv_obj_t* bar, int value, uint32_t time) {
    int start = lv_bar_get_value(bar);
    int end = value;
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, bar);
    lv_anim_set_time(&anim, time);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* bar = lv_anim_get_var(a);
        int current = start + (end - start) * a->act_time / a->time;
        lv_bar_set_value(bar, current, LV_ANIM_OFF);
    });
    lv_anim_set_values(&anim, start, end);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}
```

### Gauge/Needle Animation

```cpp
class GaugeAnimations {
public:
    // Smooth needle movement
    static void animateTo(lv_obj_t* arc, int value, uint32_t time = 1000);
};

void GaugeAnimations::animateTo(lv_obj_t* arc, int value, uint32_t time) {
    int start = lv_arc_get_value(arc);
    int end = value;
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, arc);
    lv_anim_set_time(&anim, time);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* arc = lv_anim_get_var(a);
        int current = start + (end - start) * a->act_time / a->time;
        lv_arc_set_value(arc, current);
    });
    lv_anim_set_values(&anim, start, end);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_start(&anim);
}
```

### Counter Animation (Numbers)

```cpp
class CounterAnimations {
public:
    // Animate number changes
    static void animateTo(lv_obj_t* label, int newValue, uint32_t time = 500);
    static void animateTo(lv_obj_t* label, float newValue, uint32_t time = 500);
};

void CounterAnimations::animateTo(lv_obj_t* label, int newValue, uint32_t time) {
    // Parse current value from label
    int startValue = atoi(lv_label_get_text(label));
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, label);
    lv_anim_set_time(&anim, time);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* label = lv_anim_get_var(a);
        int current = startValue + (newValue - startValue) * a->act_time / a->time;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", current);
        lv_label_set_text(label, buf);
    });
    lv_anim_set_values(&anim, startValue, newValue);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void CounterAnimations::animateTo(lv_obj_t* label, float newValue, uint32_t time) {
    float startValue = atof(lv_label_get_text(label));
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, label);
    lv_anim_set_time(&anim, time);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* label = lv_anim_get_var(a);
        float current = startValue + (newValue - startValue) * a->act_time / a->time;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", current);
        lv_label_set_text(label, buf);
    });
    lv_anim_set_values(&anim, (int32_t)(startValue * 100), (int32_t)(newValue * 100));
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}
```

---

## List Animations

### List Item Insertion/Removal

```cpp
class ListAnimations {
public:
    // Animate item insertion
    static void insert(lv_obj_t* list, lv_obj_t* item, int position, uint32_t time = 300);
    
    // Animate item removal
    static void remove(lv_obj_t* list, lv_obj_t* item, uint32_t time = 300);
    
    // Reorder items with animation
    static void reorder(lv_obj_t* list, int from, int to, uint32_t time = 300);
};

void ListAnimations::insert(lv_obj_t* list, lv_obj_t* item, int position, uint32_t time) {
    // Insert at position
    lv_obj_set_height(item, 0);
    lv_obj_set_style_opa(item, 0, 0);
    lv_obj_set_pos(item, 0, position * 60); // Assuming 60px items
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, item);
    lv_anim_set_time(&anim, time);
    lv_anim_set_values(&anim, 0, 60);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* item = lv_anim_get_var(a);
        lv_obj_set_height(item, a->act_time * 60 / a->time);
        lv_obj_set_style_opa(item, a->act_time * 255 / a->time, 0);
    });
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void ListAnimations::remove(lv_obj_t* list, lv_obj_t* item, uint32_t time) {
    int position = lv_obj_get_index(item);
    
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, item);
    lv_anim_set_time(&anim, time);
    lv_anim_set_values(&anim, 60, 0);
    lv_anim_set_exec_cb(&anim, [](lv_anim_t* a) {
        lv_obj_t* item = lv_anim_get_var(a);
        lv_obj_set_height(item, 60 - (a->act_time * 60 / a->time));
        lv_obj_set_style_opa(item, 255 - (a->act_time * 255 / a->time), 0);
    });
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
        lv_obj_delete(lv_anim_get_var(a));
    });
    lv_anim_start(&anim);
}
```

---

## Overlay Animations

### Quick Actions Overlay

```cpp
class QuickActionsAnimations {
public:
    static void show(lv_obj_t* overlay, lv_obj_t* content, uint32_t time = 300);
    static void hide(lv_obj_t* overlay, lv_obj_t* content, uint32_t time = 300);
};

void QuickActionsAnimations::show(lv_obj_t* overlay, lv_obj_t* content, uint32_t time) {
    // Fade in overlay
    lv_obj_set_style_opa(overlay, 0, 0);
    lv_anim_t fadeAnim;
    lv_anim_init(&fadeAnim);
    lv_anim_set_var(&fadeAnim, overlay);
    lv_anim_set_time(&fadeAnim, time);
    lv_anim_set_exec_cb(&fadeAnim, [](lv_anim_t* a) {
        lv_obj_set_style_opa(lv_anim_get_var(a), a->act_time * 160 / a->time, 0);
    });
    lv_anim_set_values(&fadeAnim, 0, 160);
    lv_anim_start(&fadeAnim);
    
    // Slide up content
    lv_obj_set_y(content, LV_VER_RES_MAX);
    lv_anim_t slideAnim;
    lv_anim_init(&slideAnim);
    lv_anim_set_var(&slideAnim, content);
    lv_anim_set_time(&slideAnim, time);
    lv_anim_set_exec_cb(&slideAnim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&slideAnim, LV_VER_RES_MAX, LV_VER_RES_MAX - 240);
    lv_anim_set_path_cb(&slideAnim, lv_anim_path_ease_out);
    lv_anim_start(&slideAnim);
}

void QuickActionsAnimations::hide(lv_obj_t* overlay, lv_obj_t* content, uint32_t time) {
    // Fade out overlay
    lv_anim_t fadeAnim;
    lv_anim_init(&fadeAnim);
    lv_anim_set_var(&fadeAnim, overlay);
    lv_anim_set_time(&fadeAnim, time);
    lv_anim_set_exec_cb(&fadeAnim, [](lv_anim_t* a) {
        lv_obj_set_style_opa(lv_anim_get_var(a), 160 - (a->act_time * 160 / a->time), 0);
    });
    lv_anim_set_values(&fadeAnim, 160, 0);
    lv_anim_start(&fadeAnim);
    
    // Slide down content
    lv_anim_t slideAnim;
    lv_anim_init(&slideAnim);
    lv_anim_set_var(&slideAnim, content);
    lv_anim_set_time(&slideAnim, time);
    lv_anim_set_exec_cb(&slideAnim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&slideAnim, lv_obj_get_y(content), LV_VER_RES_MAX);
    lv_anim_set_path_cb(&slideAnim, lv_anim_path_ease_in);
    lv_anim_start(&slideAnim);
}
```

### Notification Popup

```cpp
class NotificationPopupAnimations {
public:
    static void show(lv_obj_t* overlay, lv_obj_t* popup, uint32_t time = 200);
    static void hide(lv_obj_t* overlay, lv_obj_t* popup, uint32_t time = 200);
};

void NotificationPopupAnimations::show(lv_obj_t* overlay, lv_obj_t* popup, uint32_t time) {
    // Fade in overlay
    lv_obj_set_style_opa(overlay, 0, 0);
    lv_anim_t fadeAnim;
    lv_anim_init(&fadeAnim);
    lv_anim_set_var(&fadeAnim, overlay);
    lv_anim_set_time(&fadeAnim, time);
    lv_anim_set_exec_cb(&fadeAnim, [](lv_anim_t* a) {
        lv_obj_set_style_opa(lv_anim_get_var(a), a->act_time * 160 / a->time, 0);
    });
    lv_anim_set_values(&fadeAnim, 0, 160);
    lv_anim_start(&fadeAnim);
    
    // Scale in popup
    lv_obj_set_scale(popup, 256 * 80 / 100); // 80% scale
    lv_obj_set_style_opa(popup, 0, 0);
    
    lv_anim_t scaleAnim;
    lv_anim_init(&scaleAnim);
    lv_anim_set_var(&scaleAnim, popup);
    lv_anim_set_time(&scaleAnim, time);
    lv_anim_set_exec_cb(&scaleAnim, [](lv_anim_t* a) {
        lv_obj_t* popup = lv_anim_get_var(a);
        int scale = 256 * 80 / 100 + (256 * 20 / 100) * a->act_time / a->time;
        lv_obj_set_scale(popup, scale);
        lv_obj_set_style_opa(popup, a->act_time * 255 / a->time, 0);
    });
    lv_anim_set_values(&scaleAnim, 0, 100);
    lv_anim_set_path_cb(&scaleAnim, lv_anim_path_overshoot);
    lv_anim_start(&scaleAnim);
}

void NotificationPopupAnimations::hide(lv_obj_t* overlay, lv_obj_t* popup, uint32_t time) {
    // Fade out overlay
    lv_anim_t fadeAnim;
    lv_anim_init(&fadeAnim);
    lv_anim_set_var(&fadeAnim, overlay);
    lv_anim_set_time(&fadeAnim, time);
    lv_anim_set_exec_cb(&fadeAnim, [](lv_anim_t* a) {
        lv_obj_set_style_opa(lv_anim_get_var(a), 160 - (a->act_time * 160 / a->time), 0);
    });
    lv_anim_set_values(&fadeAnim, 160, 0);
    lv_anim_start(&fadeAnim);
    
    // Scale out popup
    lv_anim_t scaleAnim;
    lv_anim_init(&scaleAnim);
    lv_anim_set_var(&scaleAnim, popup);
    lv_anim_set_time(&scaleAnim, time);
    lv_anim_set_exec_cb(&scaleAnim, [](lv_anim_t* a) {
        lv_obj_t* popup = lv_anim_get_var(a);
        int scale = 256 * 100 / 100 - (256 * 100 / 100) * a->act_time / a->time;
        lv_obj_set_scale(popup, scale);
        lv_obj_set_style_opa(popup, 255 - (a->act_time * 255 / a->time), 0);
    });
    lv_anim_set_values(&scaleAnim, 100, 0);
    lv_anim_set_path_cb(&scaleAnim, lv_anim_path_ease_in);
    lv_anim_start(&scaleAnim);
}
```

---

## Spinner/Loading Animations

### Spinner Animation

```cpp
class SpinnerAnimation {
public:
    SpinnerAnimation(lv_obj_t* spinner, const void** frames, int frameCount);
    ~SpinnerAnimation();
    
    void start();
    void stop();
    bool isRunning() const;
    
private:
    lv_obj_t* spinner_;
    const void** frames_;
    int frameCount_;
    lv_anim_t anim_;
    int currentFrame_;
    
    static void animCallback(lv_anim_t* a);
};

// Implementation
SpinnerAnimation::SpinnerAnimation(lv_obj_t* spinner, const void** frames, int frameCount)
    : spinner_(spinner), frames_(frames), frameCount_(frameCount), currentFrame_(0) {
}

void SpinnerAnimation::start() {
    lv_anim_init(&anim_);
    lv_anim_set_var(&anim_, this);
    lv_anim_set_time(&anim_, 100); // 100ms per frame
    lv_anim_set_repeat_count(&anim_, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_, animCallback);
    lv_anim_start(&anim_);
}

void SpinnerAnimation::stop() {
    lv_anim_del(&anim_, nullptr);
}

void SpinnerAnimation::animCallback(lv_anim_t* a) {
    SpinnerAnimation* spinner = static_cast<SpinnerAnimation*>(lv_anim_get_var(a));
    
    spinner->currentFrame_ = (spinner->currentFrame_ + 1) % spinner->frameCount_;
    lv_img_set_src(spinner->spinner_, spinner->frames_[spinner->currentFrame_]);
}

bool SpinnerAnimation::isRunning() const {
    return lv_anim_get_var(&anim_) != nullptr;
}

SpinnerAnimation::~SpinnerAnimation() {
    stop();
}
```

---

## Custom Animations

### Color Pulse Animation

```cpp
class ColorPulseAnimation {
public:
    ColorPulseAnimation(lv_obj_t* obj, lv_color_t from, lv_color_t to);
    ~ColorPulseAnimation();
    
    void start(uint32_t time = 1000);
    void stop();
    
private:
    lv_obj_t* obj_;
    lv_color_t from_;
    lv_color_t to_;
    lv_anim_t anim_;
    
    static void animCallback(lv_anim_t* a);
};

void ColorPulseAnimation::start(uint32_t time) {
    lv_anim_init(&anim_);
    lv_anim_set_var(&anim_, this);
    lv_anim_set_time(&anim_, time);
    lv_anim_set_repeat_count(&anim_, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_, animCallback);
    lv_anim_set_values(&anim_, 0, 100);
    lv_anim_set_path_cb(&anim_, lv_anim_path_ease_in_out);
    lv_anim_start(&anim_);
}

void ColorPulseAnimation::animCallback(lv_anim_t* a) {
    ColorPulseAnimation* pulse = static_cast<ColorPulseAnimation*>(lv_anim_get_var(a));
    
    float progress = a->act_time / (float)a->time;
    
    if (progress < 0.5) {
        // First half: from -> to
        progress *= 2;
    } else {
        // Second half: to -> from
        progress = 2 - progress * 2;
    }
    
    // Interpolate color
    lv_color_t color;
    color.full = from_.full + (int32_t)(progress * (to_.full - from_.full));
    
    lv_obj_set_style_bg_color(pulse->obj_, color, 0);
}
```

### Shake Animation

```cpp
class ShakeAnimation {
public:
    ShakeAnimation(lv_obj_t* obj);
    ~ShakeAnimation();
    
    void start(uint32_t time = 500, int amplitude = 10);
    void stop();
    
private:
    lv_obj_t* obj_;
    lv_anim_t anim_;
    int amplitude_;
    
    static void animCallback(lv_anim_t* a);
};

void ShakeAnimation::start(uint32_t time, int amplitude) {
    amplitude_ = amplitude;
    
    lv_anim_init(&anim_);
    lv_anim_set_var(&anim_, this);
    lv_anim_set_time(&anim_, time);
    lv_anim_set_exec_cb(&anim_, animCallback);
    lv_anim_set_values(&anim_, 0, 100);
    lv_anim_set_path_cb(&anim_, lv_anim_path_bounce);
    lv_anim_set_ready_cb(&anim_, [](lv_anim_t* a) {
        lv_obj_set_x(lv_anim_get_var(a), 0);
    });
    lv_anim_start(&anim_);
}

void ShakeAnimation::animCallback(lv_anim_t* a) {
    ShakeAnimation* shake = static_cast<ShakeAnimation*>(lv_anim_get_var(a));
    
    // Calculate shake offset based on bounce
    int offset = shake->amplitude_ * sin(a->act_time * PI * 2 / a->time);
    lv_obj_set_x(shake->obj_, offset);
}
```

---

## Animation Sequences

### Chaining Animations

```cpp
class AnimationSequence {
public:
    AnimationSequence();
    ~AnimationSequence();
    
    void addAnimation(lv_anim_t* anim);
    void start();
    void stop();
    
private:
    std::vector<lv_anim_t*> animations_;
    size_t currentIndex_;
    
    static void animCompleteCallback(lv_anim_t* a);
};

void AnimationSequence::addAnimation(lv_anim_t* anim) {
    animations_.push_back(anim);
    
    // Set completion callback for all animations except last
    if (animations_.size() > 1) {
        lv_anim_set_ready_cb(anim, animCompleteCallback);
    }
}

void AnimationSequence::start() {
    currentIndex_ = 0;
    lv_anim_start(animations_[currentIndex_]);
}

void AnimationSequence::animCompleteCallback(lv_anim_t* a) {
    // Find the animation in the sequence
    for (size_t i = 0; i < animations_.size(); i++) {
        if (animations_[i] == a) {
            // Start next animation
            if (i + 1 < animations_.size()) {
                lv_anim_start(animations_[i + 1]);
            }
            break;
        }
    }
}

void AnimationSequence::stop() {
    for (auto* anim : animations_) {
        lv_anim_del(anim, nullptr);
    }
}
```

### Example: Card Appearance Sequence

```cpp
void animateCardIn(lv_obj_t* card) {
    // Create animation sequence
    AnimationSequence sequence;
    
    // Step 1: Fade in
    lv_anim_t fadeAnim;
    lv_anim_init(&fadeAnim);
    lv_anim_set_var(&fadeAnim, card);
    lv_anim_set_time(&fadeAnim, 200);
    lv_anim_set_exec_cb(&fadeAnim, [](lv_anim_t* a) {
        lv_obj_set_style_opa(lv_anim_get_var(a), a->act_time * 255 / a->time, 0);
    });
    lv_anim_set_values(&fadeAnim, 0, 255);
    sequence.addAnimation(&fadeAnim);
    
    // Step 2: Slide up
    lv_anim_t slideAnim;
    lv_anim_init(&slideAnim);
    lv_anim_set_var(&slideAnim, card);
    lv_anim_set_time(&slideAnim, 300);
    lv_anim_set_exec_cb(&slideAnim, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&slideAnim, 50, 0);
    lv_anim_set_path_cb(&slideAnim, lv_anim_path_ease_out);
    sequence.addAnimation(&slideAnim);
    
    // Step 3: Scale to full size
    lv_anim_t scaleAnim;
    lv_anim_init(&scaleAnim);
    lv_anim_set_var(&scaleAnim, card);
    lv_anim_set_time(&scaleAnim, 200);
    lv_anim_set_exec_cb(&scaleAnim, [](lv_anim_t* a) {
        lv_obj_t* card = lv_anim_get_var(a);
        int scale = 256 * 90 / 100 + (256 * 10 / 100) * a->act_time / a->time;
        lv_obj_set_scale(card, scale);
    });
    lv_anim_set_values(&scaleAnim, 0, 100);
    lv_anim_set_path_cb(&scaleAnim, lv_anim_path_ease_out);
    sequence.addAnimation(&scaleAnim);
    
    // Start sequence
    sequence.start();
}
```

---

## Performance Considerations

### Optimization Tips

1. **Limit Concurrent Animations**: Maximum 4-5 concurrent animations
2. **Use Simple Easing**: Linear and ease-out are fastest
3. **Avoid Overlapping**: Don't animate widgets that aren't visible
4. **Reuse Animations**: Create animations once, reuse them
5. **Hardware Acceleration**: Use if available (DMA, GPU)

### Memory Management

```cpp
class AnimationPool {
public:
    static AnimationPool& getInstance();
    
    lv_anim_t* acquire();
    void release(lv_anim_t* anim);
    
private:
    std::vector<lv_anim_t*> pool_;
    
    AnimationPool();
    ~AnimationPool();
};

// Usage
lv_anim_t* anim = AnimationPool::getInstance().acquire();
lv_anim_init(anim);
// ... configure and start
AnimationPool::getInstance().release(anim);
```

### Animation Throttling

```cpp
class AnimationThrottle {
public:
    AnimationThrottle();
    
    bool canAnimate() const;
    void startAnimation();
    void endAnimation();
    
private:
    int activeCount_;
    uint32_t lastStartTime_;
    
    static const int MAX_CONCURRENT = 5;
    static const uint32_t MIN_INTERVAL = 16; // ~60fps
};

bool AnimationThrottle::canAnimate() const {
    return activeCount_ < MAX_CONCURRENT && 
           (lv_tick_get() - lastStartTime_ > MIN_INTERVAL);
}
```

---

## Animation Best Practices

### When to Animate

| Element | Animation | Duration | Easing |
|---------|-----------|----------|--------|
| Screen transitions | Slide/Fade | 200-300ms | Ease In/Out |
| Button press | Scale | 100-150ms | Ease Out |
| Progress bars | Progress | 300-500ms | Linear/Ease Out |
| Loading indicators | Spinner | Infinite | Linear |
| Notifications | Slide/Fade | 200-300ms | Ease Out |
| Overlays | Slide/Fade | 200-300ms | Ease Out |
| Counters | Number change | 300-500ms | Ease Out |
| List items | Insert/Remove | 200-300ms | Ease Out |

### When NOT to Animate

1. **First Load**: Don't animate initial screen creation
2. **Background Updates**: Don't animate data refreshes
3. **Low Battery**: Disable animations when battery is low
4. **Performance Mode**: Disable when CPU is busy
5. **User Preference**: Respect user's animation settings

### Accessibility Considerations

1. **Reduced Motion**: Provide option to disable animations
2. **Animation Duration**: Keep animations short (under 500ms)
3. **Visual Clarity**: Ensure animations don't obscure content
4. **Consistency**: Use same animations for same actions
5. **Feedback**: Always provide visual feedback for interactions

---

## Summary

The animation system provides:

1. **Screen Transitions**: Slide, fade, scale animations
2. **Widget Animations**: Button press, progress, gauge, counter animations
3. **List Animations**: Item insertion, removal, reordering
4. **Overlay Animations**: Quick actions, notifications, loading indicators
5. **Custom Animations**: Spinner, color pulse, shake
6. **Animation Sequences**: Chaining multiple animations
7. **Performance Optimization**: Animation pooling, throttling

All animations use LVGL's built-in animation system for efficiency and are designed to work within the ESP32's resource constraints.

**Default Settings**:
- Duration: 200-300ms for most animations
- Easing: Ease out for entering, ease in for exiting
- Maximum concurrent animations: 5
- Frame rate: ~60fps when possible

**Customization**: All animation durations and types can be configured in settings.
