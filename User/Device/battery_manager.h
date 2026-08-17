#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H
#include <stdint.h>
typedef struct {
    /* 最近一次 ADC 换算得到的毫伏值。 */
    uint16_t voltage_mv;
    /* 经过平滑后的 0~100 百分比。 */
    uint8_t percent;
    /* 电压低于检测阈值时为 0，通常表示调试器/外部供电。 */
    uint8_t present;
    uint8_t charging;
    uint8_t low;
    uint8_t critical;
    uint32_t updated_at_ms;
} Battery_Data_t;
/* 初始化底层电源采样并立即取得首个样本。 */
void BatteryManager_Init(void);
/* 主循环周期入口，到期后自动采样。 */
void BatteryManager_Process(void);
void BatteryManager_ForceUpdate(void);
/* 返回内部只读缓存，调用方不得保存后修改。 */
const Battery_Data_t *BatteryManager_Get(void);
/* 取出并清除一次临界低电关机请求。 */
uint8_t BatteryManager_TakeCriticalRequest(void);
#endif
