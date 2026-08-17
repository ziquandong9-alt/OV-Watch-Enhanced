#include "menu_page.h"

#include "app_ui.h"
#include "lcd_init.h"
#include "lvgl.h"
#include "menu_icons.h"

#include <stdint.h>

typedef struct {
    /* 菜单项显示名称。字符串常量生命周期覆盖整个程序。 */
    const char *name;
    /* 私用区图标或 LVGL 内建符号。 */
    const char *icon_text;
    /* 每种图标来自不同字号/字库，绘制时必须配套使用。 */
    const lv_font_t *icon_font;
    /* 图标圆形背景色，格式为 0xRRGGBB。 */
    uint32_t icon_color;
    /* 点击后交给 AppUI 状态机的目标页面。 */
    AppUI_Page_t target_page;
} MenuItem_t;

static lv_obj_t *s_menu_list;

/* 表驱动菜单：新增功能只需增加一行，不必复制一套事件处理代码。 */
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
    /* 创建 panel 时把 MenuItem_t 地址放进 user_data，这里取回对应配置。 */
    const MenuItem_t *item = (const MenuItem_t *)lv_event_get_user_data(event);

    if((item != NULL) && (item->target_page != APP_UI_PAGE_MENU)) {
        AppUI_RequestPage(item->target_page);
    }
}

/* 在 panel 主绘制阶段直接画圆形图标和名称，避免额外子对象。 */
static void MenuPage_ItemDrawEvent(lv_event_t *event)
{
    const MenuItem_t *item;
    lv_obj_t *panel;
    lv_draw_ctx_t *draw_ctx;
    lv_draw_rect_dsc_t circle_dsc;
    lv_draw_label_dsc_t label_dsc;
    lv_area_t panel_area;
    lv_area_t draw_area;
    lv_coord_t line_height;

    /* DRAW_MAIN 阶段直接画图标和文字，减少子对象数量和滑动时的遍历开销。 */
    item = (const MenuItem_t *)lv_event_get_user_data(event);
    panel = lv_event_get_target(event);
    draw_ctx = lv_event_get_draw_ctx(event);
    if((item == NULL) || (panel == NULL) || (draw_ctx == NULL)) return;

    /* 对象坐标已经包含滚动偏移，所有手绘元素都应以它为基准。 */
    lv_obj_get_coords(panel, &panel_area);

    /* 直接画彩色圆形，替代“圆形子对象 + 图标 label”，降低对象树复杂度。 */
    draw_area.x1 = panel_area.x1 + 12;
    draw_area.x2 = draw_area.x1 + 39;
    draw_area.y1 = panel_area.y1 + 15;
    draw_area.y2 = draw_area.y1 + 39;
    lv_draw_rect_dsc_init(&circle_dsc);
    circle_dsc.radius = LV_RADIUS_CIRCLE;
    circle_dsc.bg_color = lv_color_hex(item->icon_color);
    circle_dsc.bg_opa = LV_OPA_COVER;
    circle_dsc.border_opa = LV_OPA_TRANSP;
    lv_draw_rect(draw_ctx, &circle_dsc, &draw_area);

    /* 图标字形需要按字体行高垂直居中，不能假设所有字体高度相同。 */
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = item->icon_font;
    label_dsc.color = lv_color_hex(0xFFFFFFU);
    label_dsc.opa = LV_OPA_COVER;
    label_dsc.align = LV_TEXT_ALIGN_CENTER;
    line_height = lv_font_get_line_height(item->icon_font);
    draw_area.x1 = panel_area.x1 + 12;
    draw_area.x2 = draw_area.x1 + 39;
    draw_area.y1 = panel_area.y1 + (70 - line_height) / 2;
    draw_area.y2 = draw_area.y1 + line_height - 1;
    lv_draw_label(draw_ctx, &label_dsc, &draw_area, item->icon_text, NULL);

    /* 复用 label_dsc 绘制名称，只替换字体、颜色、对齐方式和区域。 */
    label_dsc.font = &lv_font_montserrat_14;
    label_dsc.color = lv_color_hex(0xF2F4F7U);
    label_dsc.align = LV_TEXT_ALIGN_LEFT;
    line_height = lv_font_get_line_height(label_dsc.font);
    draw_area.x1 = panel_area.x1 + 68;
    draw_area.x2 = panel_area.x2 - 12;
    draw_area.y1 = panel_area.y1 + (70 - line_height) / 2;
    draw_area.y2 = draw_area.y1 + line_height - 1;
    lv_draw_label(draw_ctx, &label_dsc, &draw_area, item->name, NULL);
}

static void MenuPage_CreateItem(const MenuItem_t *item)
{
    lv_obj_t *panel;

    /* 每项只保留一个可点击 panel；视觉内容由 DRAW_MAIN 回调完成。 */
    panel = lv_obj_create(s_menu_list);
    lv_obj_set_size(panel, 240, 70);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    /* 平时透明，避免与屏幕黑色背景重复填充；按下时才显示反馈底色。 */
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel,
                              lv_color_hex(0x303238U),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(panel,
                            LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(panel,
                        MenuPage_ItemEvent,
                        LV_EVENT_CLICKED,
                        (void *)item);
    lv_obj_add_event_cb(panel,
                        MenuPage_ItemDrawEvent,
                        LV_EVENT_DRAW_MAIN,
                        (void *)item);
}

/* 创建透明纵向 flex 列表，并按静态配置表生成全部菜单项。 */
void MenuPage_Create(void)
{
    lv_obj_t *screen = lv_scr_act();
    uint32_t i;

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    /* 使用 LCD 当前逻辑宽高；本工程竖屏方向为 240×280。 */
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
    /* 根屏幕已经填充同样的纯色背景，列表透明可让每个脏像素只填充一次。 */
    lv_obj_set_style_bg_opa(s_menu_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_menu_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_menu_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_menu_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_menu_list, LV_SCROLLBAR_MODE_OFF);
    /* 禁用回弹，避免边缘区域在越界动画中被反复整块重绘。 */
		lv_obj_clear_flag(s_menu_list, LV_OBJ_FLAG_SCROLL_ELASTIC);

    /* 用 sizeof 自动计算元素个数，增删菜单项时不需要同步维护常量。 */
    for(i = 0U; i < (sizeof(s_menu_items) / sizeof(s_menu_items[0])); i++) {
        MenuPage_CreateItem(&s_menu_items[i]);
    }
}

void MenuPage_Destroy(void)
{
    /* 子 panel 会由 clean 递归删除，只需把模块保存的根指针清空。 */
    lv_obj_clean(lv_scr_act());
    s_menu_list = NULL;
}
