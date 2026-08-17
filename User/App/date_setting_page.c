#include "date_setting_page.h"

#include "app_ui.h"
#include "device_manager.h"
#include "lvgl.h"
#include "rtc.h"

#include <stdint.h>

/* 三个 roller 指针由 Create 建立、Destroy 清空；页面外不得长期持有它们。 */
static lv_obj_t *s_year_roller;
static lv_obj_t *s_month_roller;
static lv_obj_t *s_day_roller;
static lv_obj_t *s_status_label;
/* LVGL roller 用换行分隔选项；这些静态缓冲区必须在 roller 存活期间一直有效。 */
static char s_year_options[500];
static char s_month_options[36];
static char s_day_options[93];
/* 修改月份时会重建日期选项；该标志防止 VALUE_CHANGED 递归进入更新函数。 */
static uint8_t s_updating_day;

static void DateSettingPage_MakeOptions(char *buffer,
                                        uint16_t first,
                                        uint16_t last,
                                        uint8_t four_digits);
static uint8_t DateSettingPage_DaysInMonth(uint16_t year, uint8_t month);
static uint8_t DateSettingPage_Weekday(uint16_t year,
                                       uint8_t month,
                                       uint8_t day);
static void DateSettingPage_UpdateDays(void);
static void DateSettingPage_DateChanged(lv_event_t *event);
static void DateSettingPage_Confirm(lv_event_t *event);
static lv_obj_t *DateSettingPage_CreateRoller(lv_obj_t *screen,
                                              const char *options,
                                              lv_coord_t x);

/* 创建年月日 roller、恢复 RTC 当前日期并绑定联动事件。 */
void DateSettingPage_Create(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *title;
    lv_obj_t *confirm_button;
    lv_obj_t *confirm_label;

    /* 页面独占活动屏幕，先清理旧对象并让根屏幕本身不参与滚动。 */
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    /* 在运行时生成选项，可避免维护一大段容易写错的年份字符串。 */
    DateSettingPage_MakeOptions(s_year_options, 2000U, 2099U, 1U);
    DateSettingPage_MakeOptions(s_month_options, 1U, 12U, 0U);

    title = lv_label_create(screen);
    lv_label_set_text(title, "SET DATE");
    lv_obj_set_style_text_color(title, lv_color_hex(0xDCE2EAU), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 9);

    s_year_roller = DateSettingPage_CreateRoller(screen, s_year_options, 5);
    s_month_roller = DateSettingPage_CreateRoller(screen, s_month_options, 86);
    s_day_roller = DateSettingPage_CreateRoller(screen, "01", 167);

    /* STM32 RTC 要先读 Time 再读 Date，才能正确解锁影子寄存器。 */
    (void)HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    if(HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
        date.Year = 0U;
        date.Month = 1U;
        date.Date = 1U;
    }

    /* RTC.Year 是从 2000 年开始的 0~99，正好对应年份 roller 索引。 */
    lv_roller_set_selected(s_year_roller, date.Year, LV_ANIM_OFF);
    lv_roller_set_selected(s_month_roller, date.Month - 1U, LV_ANIM_OFF);
    /* 必须先按年份/月生成正确的天数，再选择原来的日期。 */
    DateSettingPage_UpdateDays();
    lv_roller_set_selected(s_day_roller, date.Date - 1U, LV_ANIM_OFF);

    lv_obj_add_event_cb(s_year_roller,
                        DateSettingPage_DateChanged,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);
    lv_obj_add_event_cb(s_month_roller,
                        DateSettingPage_DateChanged,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);

    confirm_button = lv_btn_create(screen);
    lv_obj_set_size(confirm_button, 118, 38);
    lv_obj_align(confirm_button, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_radius(confirm_button, 19, LV_PART_MAIN);
    lv_obj_set_style_bg_color(confirm_button,
                              lv_color_hex(0x2878FFU),
                              LV_PART_MAIN);
    lv_obj_add_event_cb(confirm_button,
                        DateSettingPage_Confirm,
                        LV_EVENT_CLICKED,
                        NULL);

    confirm_label = lv_label_create(confirm_button);
    lv_label_set_text(confirm_label, "CONFIRM");
    lv_obj_center(confirm_label);

    s_status_label = lv_label_create(screen);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label,
                                lv_color_hex(0xFF7070U),
                                LV_PART_MAIN);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -49);
}

void DateSettingPage_Destroy(void)
{
    /* clean 会递归删除子对象；随后清空指针，避免误访问已释放的 LVGL 对象。 */
    lv_obj_clean(lv_scr_act());
    s_year_roller = NULL;
    s_month_roller = NULL;
    s_day_roller = NULL;
    s_status_label = NULL;
    s_updating_day = 0U;
}

static lv_obj_t *DateSettingPage_CreateRoller(lv_obj_t *screen,
                                              const char *options,
                                              lv_coord_t x)
{
    /* 统一封装 roller 样式，保证年月日三列的尺寸和选中效果一致。 */
    lv_obj_t *roller = lv_roller_create(screen);

    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 3U);
    lv_obj_set_size(roller, 68, 112);
    lv_obj_align(roller, LV_ALIGN_TOP_LEFT, x, 38);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x10151CU), LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller,
                              lv_color_hex(0x2878FFU),
                              LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller,
                                lv_color_hex(0x87909BU),
                                LV_PART_MAIN);
    lv_obj_set_style_text_color(roller,
                                lv_color_hex(0xFFFFFFU),
                                LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 8, LV_PART_MAIN);
    return roller;
}

/* 生成由换行分隔的定宽十进制 roller 选项字符串。 */
static void DateSettingPage_MakeOptions(char *buffer,
                                        uint16_t first,
                                        uint16_t last,
                                        uint8_t four_digits)
{
    uint16_t value;
    uint16_t pos = 0U;

    /* 直接逐字符写入可避免 sprintf 引入较大的格式化代码和栈开销。 */
    for(value = first; value <= last; value++) {
        if(four_digits != 0U) {
            buffer[pos++] = (char)('0' + ((value / 1000U) % 10U));
            buffer[pos++] = (char)('0' + ((value / 100U) % 10U));
        }
        buffer[pos++] = (char)('0' + ((value / 10U) % 10U));
        buffer[pos++] = (char)('0' + (value % 10U));
        /* 最后一项后不能再放换行，否则 LVGL 会额外显示一个空选项。 */
        if(value != last) {
            buffer[pos++] = '\n';
        }
    }
    buffer[pos] = '\0';
}

/* 返回指定年月的实际天数，包含完整公历闰年规则。 */
static uint8_t DateSettingPage_DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };
    uint8_t result = days[month - 1U];

    /* 公历闰年：400 的倍数，或能被 4 整除但不能被 100 整除。 */
    if((month == 2U) &&
       (((year % 400U) == 0U) ||
        (((year % 4U) == 0U) && ((year % 100U) != 0U)))) {
        result = 29U;
    }
    return result;
}

static void DateSettingPage_UpdateDays(void)
{
    /* roller 返回从 0 开始的索引，因此月份和日期读取后都要加 1。 */
    uint16_t year = 2000U + lv_roller_get_selected(s_year_roller);
    uint8_t month = (uint8_t)(lv_roller_get_selected(s_month_roller) + 1U);
    uint8_t old_day = (uint8_t)(lv_roller_get_selected(s_day_roller) + 1U);
    uint8_t day_count = DateSettingPage_DaysInMonth(year, month);

    /* 例如从 1 月 31 日切到 2 月，自动夹紧为 2 月最后一天。 */
    if(old_day > day_count) {
        old_day = day_count;
    }

    /* set_options 可能触发值变化事件，用保护标志阻止递归刷新。 */
    s_updating_day = 1U;
    DateSettingPage_MakeOptions(s_day_options, 1U, day_count, 0U);
    lv_roller_set_options(s_day_roller, s_day_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(s_day_roller, old_day - 1U, LV_ANIM_OFF);
    s_updating_day = 0U;
}

static void DateSettingPage_DateChanged(lv_event_t *event)
{
    /* 年或月变化时重算该月天数；内部更新产生的事件被保护标志忽略。 */
    (void)event;
    if(s_updating_day == 0U) {
        DateSettingPage_UpdateDays();
    }
}

/* 计算 RTC 需要的星期枚举。 */
static uint8_t DateSettingPage_Weekday(uint16_t year,
                                       uint8_t month,
                                       uint8_t day)
{
    static const uint8_t month_table[] = {
        0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U
    };
    uint32_t y = year;
    uint32_t weekday;

    /* Sakamoto 算法把一、二月看作上一年的第 13、14 月。 */
    if(month < 3U) {
        y--;
    }
    weekday = (y + y / 4U - y / 100U + y / 400U +
               month_table[month - 1U] + day) % 7U;
    /* 算法中 0 表示周日，而 STM32 HAL 用常量 RTC_WEEKDAY_SUNDAY。 */
    return (weekday == 0U) ? RTC_WEEKDAY_SUNDAY : (uint8_t)weekday;
}

static void DateSettingPage_Confirm(lv_event_t *event)
{
    /* 把三个 roller 索引转成 RTC 日期结构并提交保存。 */
    RTC_DateTypeDef date = {0};
    uint16_t year;

    (void)event;
    year = 2000U + lv_roller_get_selected(s_year_roller);
    date.Year = (uint8_t)(year - 2000U);
    date.Month = (uint8_t)(lv_roller_get_selected(s_month_roller) + 1U);
    date.Date = (uint8_t)(lv_roller_get_selected(s_day_roller) + 1U);
    /* RTC 不会替我们推算星期，保存日期时必须同步写入 WeekDay。 */
    date.WeekDay = DateSettingPage_Weekday(year, date.Month, date.Date);

    if(HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK) {
        /* RTC 成功后再持久化；写失败时仍留在本页并显示错误。 */
        DeviceManager_SaveDateTimeNow();
        AppUI_RequestPage(APP_UI_PAGE_MENU);
    }
    else {
        lv_label_set_text(s_status_label, "RTC ERROR");
    }
}
