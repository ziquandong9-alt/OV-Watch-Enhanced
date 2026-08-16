#include "menu_page.h"

#include "app_ui.h"
#include "lcd_init.h"
#include "lvgl.h"
#include "menu_icons.h"

#include <stdint.h>

typedef struct {
    const char *name;
    const char *icon_text;
    const lv_font_t *icon_font;
    uint32_t icon_color;
    AppUI_Page_t target_page;
} MenuItem_t;

static lv_obj_t *s_menu_list;

/* Icon glyphs and colors are taken from the original OV-Watch menu. */
static const MenuItem_t s_menu_items[] = {
    {"Calendar",     MENU_ICON_CALENDAR,     &ui_font_iconfont30, 0xFF8080U, APP_UI_PAGE_DATE_SETTING},
    {"Stopwatch",    MENU_ICON_TIMER,        &ui_font_iconfont34, 0x3A9DFFU, APP_UI_PAGE_STOPWATCH},
    {"Calculator",   MENU_ICON_CALCULATOR,   &lv_font_montserrat_20, 0xF39A2EU, APP_UI_PAGE_CALCULATOR},
    {"Steps",        MENU_ICON_TIMER,        &ui_font_iconfont34, 0xDC80E6U, APP_UI_PAGE_MOTION},
    {"Environment",  MENU_ICON_ENVIRONMENT,  &ui_font_iconfont34, 0x009632U, APP_UI_PAGE_ENVIRONMENT},
    {"Heart Rate",   MENU_ICON_HEART_RATE,   &ui_font_iconfont34, 0xC80000U, APP_UI_PAGE_HEART},
    {"Compass",      MENU_ICON_COMPASS,      &ui_font_iconfont34, 0x800080U, APP_UI_PAGE_COMPASS},
    {"Battery",      LV_SYMBOL_BATTERY_FULL, &lv_font_montserrat_20, 0x39C66DU, APP_UI_PAGE_BATTERY},
    {"History",      LV_SYMBOL_LIST,         &lv_font_montserrat_20, 0x627DFFU, APP_UI_PAGE_HISTORY},
    {"Settings",     MENU_ICON_SETTINGS,     &ui_font_iconfont30, 0x808080U, APP_UI_PAGE_SETTINGS},
    {"About",        MENU_ICON_ABOUT,        &ui_font_iconfont30, 0x646464U, APP_UI_PAGE_ABOUT},
};

static void MenuPage_ItemEvent(lv_event_t *event)
{
    const MenuItem_t *item = (const MenuItem_t *)lv_event_get_user_data(event);

    if((item != NULL) && (item->target_page != APP_UI_PAGE_MENU)) {
        AppUI_RequestPage(item->target_page);
    }
}

static void MenuPage_CreateItem(const MenuItem_t *item)
{
    lv_obj_t *panel;
    lv_obj_t *icon_circle;
    lv_obj_t *icon;
    lv_obj_t *name;
    lv_obj_t *arrow;

    panel = lv_obj_create(s_menu_list);
    lv_obj_set_size(panel, 240, 70);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel,
                              lv_color_hex(0x808080U),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(panel,
                            100,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(panel,
                        MenuPage_ItemEvent,
                        LV_EVENT_CLICKED,
                        (void *)item);

    icon_circle = lv_obj_create(panel);
    lv_obj_set_size(icon_circle, 40, 40);
    lv_obj_align(icon_circle, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_clear_flag(icon_circle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(icon_circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_circle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(icon_circle, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(icon_circle,
                              lv_color_hex(item->icon_color),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(icon_circle, LV_OPA_COVER, LV_PART_MAIN);

    icon = lv_label_create(icon_circle);
    lv_label_set_text(icon, item->icon_text);
    lv_obj_set_style_text_font(icon, item->icon_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFFU), LV_PART_MAIN);
    lv_obj_center(icon);

    name = lv_label_create(panel);
    lv_label_set_text(name, item->name);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(name, lv_color_hex(0xF2F4F7U), LV_PART_MAIN);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 68, 0);

    arrow = lv_label_create(panel);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x6F7884U), LV_PART_MAIN);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -12, 0);
}

void MenuPage_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    uint32_t i;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    /* Use the active LCD geometry; portrait mode is 240 x 280. */
    s_menu_list = lv_obj_create(screen);
    lv_obj_set_size(s_menu_list, LCD_W, LCD_H);
    lv_obj_center(s_menu_list);
    lv_obj_set_flex_flow(s_menu_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_menu_list,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_menu_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_menu_list, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_menu_list, lv_color_hex(0x05070AU), LV_PART_MAIN);
    /* The screen already paints the same opaque background. Keep the scrolling
       container transparent so every invalidated pixel is filled only once. */
    lv_obj_set_style_bg_opa(s_menu_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_menu_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_menu_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_menu_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_menu_list, LV_SCROLLBAR_MODE_OFF);
		lv_obj_clear_flag(s_menu_list, LV_OBJ_FLAG_SCROLL_ELASTIC);

    for(i = 0U; i < (sizeof(s_menu_items) / sizeof(s_menu_items[0])); i++) {
        MenuPage_CreateItem(&s_menu_items[i]);
    }
}

void MenuPage_Destroy(void)
{
    lv_obj_clean(lv_scr_act());
    s_menu_list = NULL;
}
