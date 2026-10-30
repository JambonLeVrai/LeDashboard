#ifndef PAGE_BASE_H
#define PAGE_BASE_H

#include <lvgl.h>
#include <stddef.h>

class PageBase {
public:
    PageBase();
    virtual ~PageBase();
    
    // Create the page (called when page is shown)
    virtual void create(lv_obj_t* parent) = 0;
    
    // Destroy the page (called when page is hidden)
    virtual void destroy();
    
    // Update the page with new data
    virtual void update() = 0;
    
    // Get the page container
    lv_obj_t* getContainer() const { return page_container; }
    
    // Set active state
    void setActive(bool active) { page_active = active; }
    bool isActive() const { return page_active; }

protected:
    lv_obj_t* page_container;
    bool page_active;
    
    // Common styles
    lv_style_t page_style_main;
    lv_style_t page_style_label;
};

#endif // PAGE_BASE_H
