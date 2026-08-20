#include "status_bar.h"
#include "battery_manager.h"
#include "ble_manager.h"

static lv_obj_t *s_bar;
static lv_obj_t *s_ble;
static lv_obj_t *s_battery;
static lv_timer_t *s_timer;

static void Refresh(lv_timer_t *timer)
{
    /* BatteryManager 返回内部缓存的只读地址，不要在这里修改数据。 */
    const Battery_Data_t *data = BatteryManager_Get();
    if((s_bar == NULL) || (s_ble == NULL) || (s_battery == NULL)) return;
    /* 页面切换可能已删除对象但尚未来得及销毁本定时器，因此再次验证对象。 */
    if((lv_obj_is_valid(s_bar) == false) ||
       (lv_obj_is_valid(s_ble) == false) ||
       (lv_obj_is_valid(s_battery) == false)) {
        s_bar = s_ble = s_battery = NULL;
        /* 暂停失去目标的 timer，防止每 2 秒继续访问无效对象。 */
        if(timer != NULL) lv_timer_pause(timer);
        return;
    }
    /* 只显示连接结果，不在 UI 层直接访问 UART 或蓝牙协议状态机。 */
    lv_label_set_text(s_ble, BLEManager_IsConnected() ? "BLE" : "--");
    lv_obj_set_style_text_color(s_ble,
        BLEManager_IsConnected() ? lv_color_hex(0x45A3FFU) : lv_color_hex(0x59616DU), 0);
    /* 未检测到电池时显示外部供电；充电时在百分比前加“+”。 */
    if(data->present == 0U) lv_label_set_text(s_battery, "EXT");
    else lv_label_set_text_fmt(s_battery, data->charging ? "+%u%%" : "%u%%", data->percent);
    lv_obj_set_style_text_color(s_battery,
        data->low ? lv_color_hex(0xFF4D4FU) : lv_color_hex(0xD8DDE5U), 0);
}

void StatusBar_Create(lv_obj_t *parent)
{
    /* Create 允许重复调用：先销毁旧实例可避免遗留多个刷新定时器。 */
    StatusBar_Destroy();
    s_bar = lv_obj_create(parent);
    /* 240 像素圆角屏顶部较窄，196 像素宽可让两端文字留在安全区内。 */
    lv_obj_set_size(s_bar, 196, 18);
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_bar, 0, 0);
    lv_obj_set_style_pad_all(s_bar, 0, 0);
    s_ble = lv_label_create(s_bar);
    lv_obj_set_style_text_font(s_ble, &lv_font_montserrat_14, 0);
    lv_obj_align(s_ble, LV_ALIGN_LEFT_MID, 0, 0);
    s_battery = lv_label_create(s_bar);
    lv_obj_set_style_text_font(s_battery, &lv_font_montserrat_14, 0);
    lv_obj_align(s_battery, LV_ALIGN_RIGHT_MID, 0, 0);
    /* 状态栏无需逐帧更新；2 秒一次能兼顾电量响应与刷新成本。 */
    s_timer = lv_timer_create(Refresh, 2000U, NULL);
    /* 立即填充首屏，避免等待第一个周期时显示空 label。 */
    Refresh(NULL);
}

void StatusBar_Destroy(void)
{
    /* 必须先删 timer 再删对象，杜绝回调在对象释放后继续运行。 */
    if(s_timer != NULL) { lv_timer_del(s_timer); s_timer = NULL; }
    if((s_bar != NULL) && lv_obj_is_valid(s_bar)) lv_obj_del(s_bar);
    s_bar = s_ble = s_battery = NULL;
}

void StatusBar_SetVisible(uint8_t visible)
{
    /* HIDDEN 只影响绘制和命中，不销毁对象与刷新 timer。 */
    if((s_bar == NULL) || (lv_obj_is_valid(s_bar) == false)) return;
    if(visible != 0U) lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
}

void StatusBar_SetBatteryVisible(uint8_t visible)
{
    /* 第二表盘自带电池组件时只隐藏右侧 label，BLE 仍可保留。 */
    if((s_battery == NULL) || (lv_obj_is_valid(s_battery) == false)) return;
    if(visible != 0U) lv_obj_clear_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_battery, LV_OBJ_FLAG_HIDDEN);
}

void StatusBar_SetBleVisible(uint8_t visible)
{
    /* 子页面左上角留给触摸返回键，避免 BLE 文本与按钮重叠。 */
    if((s_ble == NULL) || (lv_obj_is_valid(s_ble) == false)) return;
    if(visible != 0U) lv_obj_clear_flag(s_ble, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_ble, LV_OBJ_FLAG_HIDDEN);
}

void StatusBar_SetUpdatesPaused(uint8_t paused)
{
    if(s_timer == NULL) return;
    if(paused != 0U) {
        lv_timer_pause(s_timer);
    }
    else {
        /* 恢复前主动同步一次数据，随后 timer 再从当前时刻开始计周期。 */
        Refresh(NULL);
        lv_timer_resume(s_timer);
    }
}
