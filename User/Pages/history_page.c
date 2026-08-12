#include "history_page.h"
#include "history_manager.h"
#include "lvgl.h"

void HistoryPage_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;
    lv_obj_t *list;
    lv_obj_t *row;
    lv_obj_t *label;
    History_Day_t day;
    uint8_t i;
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    title = lv_label_create(screen);
    lv_label_set_text(title, "7-Day History");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 23);
    list = lv_obj_create(screen);
    lv_obj_set_size(list, 230, 222);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_style_pad_row(list, 3, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    for(i = 0U; i < HistoryManager_GetCount(); i++) {
        if(HistoryManager_GetNewest(i, &day) == 0U) continue;
        row = lv_obj_create(list);
        lv_obj_set_size(row, 218, 43);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x15191FU), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 5, 0);
        label = lv_label_create(row);
        lv_label_set_text_fmt(label, "%02u/%02u  %u steps\nHR %u  %dC  RH %u%%",
                              day.month, day.day, day.steps,
                              day.average_heart_rate,
                              (int)day.temperature_c, day.humidity_percent);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xD7DCE3U), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 2, 0);
    }
    if(HistoryManager_GetCount() == 0U) {
        label = lv_label_create(list);
        lv_label_set_text(label, "Daily records appear after midnight.");
        lv_obj_set_style_text_color(label, lv_color_hex(0x8D96A3U), 0);
    }
}

void HistoryPage_Destroy(void) { lv_obj_clean(lv_scr_act()); }
