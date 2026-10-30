#include "ui/page_base.h"

PageBase::PageBase() : page_container(nullptr), page_active(false) {
    // Initialize styles
    lv_style_init(&page_style_main);
    lv_style_set_bg_opa(&page_style_main, LV_OPA_TRANSP);
    
    lv_style_init(&page_style_label);
    lv_style_set_text_color(&page_style_label, lv_color_white());
}

PageBase::~PageBase() {
    destroy();
}

void PageBase::destroy() {
    if (page_container != nullptr) {
        lv_obj_del(page_container);
        page_container = nullptr;
    }
    page_active = false;
}
