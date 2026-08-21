#include "notification_page.h"

#include "lvgl.h"
#include "notification_manager.h"

#include <stdint.h>

static lv_obj_t *s_list;
static lv_obj_t *s_count_label;
static lv_obj_t *s_root;
static lv_timer_t *s_update_timer;
static uint32_t s_seen_generation;
static uint32_t s_detail_id;
static uint32_t s_pending_detail_id;
static uint32_t s_suppress_click_id;
static uint8_t s_detail_active;
static uint8_t s_async_pending;
static uint8_t s_page_alive;

/*
 * 通知页有“列表”和“详情”两种内部视图，但对 AppUI 仍是同一个页面。
 * 删除对象树的操作通过 lv_async_call 延迟执行，避免在卡片自身事件回调中
 * 直接删除当前事件目标。page_alive 则防止页面退出后异步任务继续重建对象。
 */
static void NotificationPage_ShowList(void);
static void NotificationPage_RebuildList(void);
static void NotificationPage_RebuildAsync(void *user_data);
static void NotificationPage_ShowDetailAsync(void *user_data);
static void NotificationPage_CardClicked(lv_event_t *event);
static void NotificationPage_CardGesture(lv_event_t *event);

static uint32_t NotificationColor(Notification_Type_t type)
{
    /* 不同健康事件使用固定强调色，让用户不读文字也能快速区分来源。 */
    switch(type) {
    case NOTIFICATION_TYPE_HEART:       return 0xFF4D5AU;
    case NOTIFICATION_TYPE_ENVIRONMENT: return 0x4FCB75U;
    case NOTIFICATION_TYPE_SEDENTARY:   return 0x55B8FFU;
    case NOTIFICATION_TYPE_FALL:        return 0xFF9F43U;
    default:                            return 0x808A96U;
    }
}

static const Notification_Record_t *NotificationPage_Find(uint32_t id)
{
    /* 列表删除会改变索引，因此详情始终按稳定 id 重新查找记录。 */
    uint8_t index;
    for(index = 0U; index < NotificationManager_GetCount(); index++) {
        const Notification_Record_t *record = NotificationManager_Get(index);
        if((record != NULL) && (record->id == id)) return record;
    }
    return NULL;
}

static void NotificationPage_ScheduleList(void)
{
    /* 合并重复请求：已经安排异步重建时，不再向 LVGL 队列塞第二份任务。 */
    if(s_async_pending == 0U) {
        s_async_pending = 1U;
        lv_async_call(NotificationPage_RebuildAsync, NULL);
    }
}

static void NotificationPage_CreateCard(const Notification_Record_t *record)
{
    /* 一条通知对应一个卡片；record 内容复制进 label，user_data 只保存稳定 id。 */
    lv_obj_t *card;
    lv_obj_t *accent;
    lv_obj_t *title;
    lv_obj_t *time_label;
    lv_obj_t *summary;

    card = lv_obj_create(s_list);
    lv_obj_set_size(card, 224, 64);
    /* 卡片不自己滚动，且右滑删除手势不能继续冒泡成列表手势。 */
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111720U), LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x202A35U),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 8, LV_PART_MAIN);

    accent = lv_obj_create(card);
    lv_obj_set_size(accent, 4, 44);
    lv_obj_align(accent, LV_ALIGN_LEFT_MID, -2, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(accent,
                              lv_color_hex(NotificationColor(record->type)),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(accent, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(accent, 2, LV_PART_MAIN);

    title = lv_label_create(card);
    lv_label_set_text(title, record->title);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_size(title, 142, 19);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, -2);

    time_label = lv_label_create(card);
    lv_label_set_text_fmt(time_label, "%02u:%02u",
                          (unsigned int)record->hour,
                          (unsigned int)record->minute);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x77818CU), LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, 0, -2);

    summary = lv_label_create(card);
    lv_label_set_text(summary, record->message);
    lv_label_set_long_mode(summary, LV_LABEL_LONG_DOT);
    lv_obj_set_size(summary, 190, 20);
    lv_obj_set_style_text_font(summary, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(summary, lv_color_hex(0xA8B0B9U), LV_PART_MAIN);
    lv_obj_align(summary, LV_ALIGN_BOTTOM_LEFT, 8, 2);

    /* uintptr_t 用于在指针宽度上安全承载 32 位 id。 */
    lv_obj_add_event_cb(card, NotificationPage_CardClicked,
                        LV_EVENT_CLICKED, (void *)(uintptr_t)record->id);
    lv_obj_add_event_cb(card, NotificationPage_CardGesture,
                        LV_EVENT_GESTURE, (void *)(uintptr_t)record->id);
}

static void NotificationPage_ClearAll(lv_event_t *event)
{
    lv_indev_t *indev = lv_indev_get_act();
    (void)event;
    /* 等触摸释放后再异步重建，避免同一按压落到新建对象上。 */
    if(indev != NULL) lv_indev_wait_release(indev);
    NotificationManager_ClearAll();
    NotificationPage_ScheduleList();
}

static void NotificationPage_CreateClearButton(void)
{
    /* 按钮作为 flex 列表最后一项创建，会自然参与滚动布局。 */
    lv_obj_t *button = lv_btn_create(s_list);
    lv_obj_t *label;

    lv_obj_set_size(button, 208, 48);
    lv_obj_set_ext_click_area(button, 6);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x8F252DU), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xB9323CU),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(button, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(button, NotificationPage_ClearAll,
                        LV_EVENT_CLICKED, NULL);
    label = lv_label_create(button);
    lv_label_set_text(label, "CLEAR ALL");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(label);
}

static void NotificationPage_RebuildList(void)
{
    uint8_t index;
    uint8_t count;

    if(s_list == NULL) return;
    /* 整体重建适合最多 12 条的小列表，逻辑简单且不会保留失效卡片。 */
    lv_obj_clean(s_list);
    count = NotificationManager_GetCount();
    lv_label_set_text_fmt(s_count_label, "%u notification%s",
                          (unsigned int)count,
                          (count == 1U) ? "" : "s");

    if(count == 0U) {
        lv_obj_t *empty = lv_label_create(s_list);
        lv_label_set_text(empty, "No notifications");
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x747E89U), LV_PART_MAIN);
        lv_obj_set_style_pad_top(empty, 70, LV_PART_MAIN);
    }
    else {
        for(index = 0U; index < count; index++) {
            const Notification_Record_t *record = NotificationManager_Get(index);
            if(record != NULL) NotificationPage_CreateCard(record);
        }
        /* 清空按钮始终放在列表末尾，既不易误触又可以滚动到达。 */
        NotificationPage_CreateClearButton();
    }
    s_seen_generation = NotificationManager_GetGeneration();
}

static void NotificationPage_ShowList(void)
{
    /* 列表和详情复用 s_root；切换视图时只清理 root，不碰 AppUI 状态栏。 */
    lv_obj_t *screen = s_root;
    lv_obj_t *title;

    if(screen == NULL) return;
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    title = lv_label_create(screen);
    lv_label_set_text(title, "NOTIFICATIONS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 9, 8);

    s_count_label = lv_label_create(screen);
    lv_obj_set_style_text_font(s_count_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_count_label, lv_color_hex(0x7F8995U), LV_PART_MAIN);
    lv_obj_align(s_count_label, LV_ALIGN_TOP_LEFT, 9, 29);

    s_list = lv_obj_create(screen);
    lv_obj_set_size(s_list, 240, 232);
    lv_obj_align(s_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_list, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_list, 7, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);

    s_detail_active = 0U;
    s_detail_id = 0U;
    NotificationPage_RebuildList();
}

/* 清理列表视图并按稳定 id 创建通知详情视图。 */
static void NotificationPage_ShowDetail(uint32_t id)
{
    const Notification_Record_t *record = NotificationPage_Find(id);
    lv_obj_t *screen = s_root;
    lv_obj_t *title;
    lv_obj_t *time_label;
    lv_obj_t *panel;
    lv_obj_t *message;
    lv_obj_t *hint;

    if(screen == NULL) return;
    /* 若通知刚被外部清除，详情目标已不存在，安全退回列表。 */
    if(record == NULL) {
        NotificationPage_ShowList();
        return;
    }

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    /* clean 已删除列表对象，立即清空借用指针防止更新 timer 误用。 */
    s_list = NULL;
    s_count_label = NULL;

    title = lv_label_create(screen);
    lv_label_set_text(title, record->title);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_size(title, 164, 24);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(title,
        lv_color_hex(NotificationColor(record->type)), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_RIGHT, -8, 8);

    time_label = lv_label_create(screen);
    lv_label_set_text_fmt(time_label, "Today  %02u:%02u",
                          (unsigned int)record->hour,
                          (unsigned int)record->minute);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x78828DU), LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, -8, 31);

    panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 224, 184);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 13);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x111720U), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);

    message = lv_label_create(panel);
    lv_label_set_text(message, record->message);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(message, 194);
    lv_obj_set_style_text_font(message, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(message, lv_color_hex(0xD3D8DEU), LV_PART_MAIN);
    lv_obj_align(message, LV_ALIGN_TOP_LEFT, 0, 0);

    hint = lv_label_create(screen);
    lv_label_set_text(hint, "KEY1 or back to return");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x626C77U), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);

    s_detail_active = 1U;
    s_detail_id = id;
    s_seen_generation = NotificationManager_GetGeneration();
}

static void NotificationPage_RebuildAsync(void *user_data)
{
    (void)user_data;
    s_async_pending = 0U;
    s_suppress_click_id = 0U;
    /* 页面退出后异步回调仍可能到达，alive 检查是最后一道生命周期保护。 */
    if(s_page_alive == 0U) return;
    NotificationPage_ShowList();
}

static void NotificationPage_ShowDetailAsync(void *user_data)
{
    /* 延迟到当前事件分发结束后再删除列表并创建详情。 */
    (void)user_data;
    s_async_pending = 0U;
    if(s_page_alive == 0U) return;
    NotificationPage_ShowDetail(s_pending_detail_id);
}

static void NotificationPage_CardClicked(lv_event_t *event)
{
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(event);

    /* 右滑删除后的释放可能紧跟一个 CLICKED，用 suppress id 吃掉这次幽灵点击。 */
    if(id == s_suppress_click_id) return;
    if(s_async_pending == 0U) {
        s_pending_detail_id = id;
        s_async_pending = 1U;
        lv_async_call(NotificationPage_ShowDetailAsync, NULL);
    }
}

static void NotificationPage_CardGesture(lv_event_t *event)
{
    /* 卡片向右滑删除；先 wait_release，再修改模型并安排列表重建。 */
    lv_indev_t *indev = lv_indev_get_act();
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(event);

    if((indev != NULL) &&
       (lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT)) {
        s_suppress_click_id = id;
        lv_indev_wait_release(indev);
        if(NotificationManager_Remove(id) != 0U) {
            NotificationPage_ScheduleList();
        }
    }
}

static void NotificationPage_Update(lv_timer_t *timer)
{
    /* 每 500 ms 比较 generation，数据没变化时零对象更新、零重绘。 */
    (void)timer;
    if(NotificationManager_GetGeneration() == s_seen_generation) return;

    if(s_detail_active != 0U) {
        if(NotificationPage_Find(s_detail_id) == NULL) {
            NotificationPage_ScheduleList();
        }
        else {
            s_seen_generation = NotificationManager_GetGeneration();
        }
    }
    else {
        NotificationPage_RebuildList();
    }
}

/* 建立隔离 root、显示初始列表并启动 generation 检查 timer。 */
void NotificationPage_Create(void)
{
    lv_obj_t *screen = lv_scr_act();

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    /* 单独 root 隔离列表/详情内容，避免内部 clean 删除 AppUI 的全局状态栏。 */
    s_root = lv_obj_create(screen);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);

    s_page_alive = 1U;
    s_async_pending = 0U;
    s_suppress_click_id = 0U;
    NotificationPage_ShowList();
    /* 低频 timer 只检查版本号，不会每 500 ms 无条件重建列表。 */
    s_update_timer = lv_timer_create(NotificationPage_Update, 500U, NULL);
}

uint8_t NotificationPage_HandleBack(void)
{
    /* 返回 1 表示详情层已经消费按键；返回 0 才让 AppUI 离开通知页。 */
    if((s_page_alive == 0U) || (s_detail_active == 0U)) return 0U;
    NotificationPage_ShowList();
    return 1U;
}

void NotificationPage_Destroy(void)
{
    /* 先标记死亡、再删 timer/对象；已排队异步回调会因 alive=0 自行退出。 */
    s_page_alive = 0U;
    if(s_update_timer != NULL) {
        lv_timer_del(s_update_timer);
        s_update_timer = NULL;
    }
    lv_obj_clean(lv_scr_act());
    s_root = NULL;
    s_list = NULL;
    s_count_label = NULL;
    s_detail_active = 0U;
    s_async_pending = 0U;
}
