#ifndef HISTORY_MANAGER_H
#define HISTORY_MANAGER_H

#include <stdint.h>

/* 受 24C02 容量限制，只保留最近七天。 */
#define HISTORY_MAX_DAYS 7U

typedef struct {
    /* 槽通过双校验后才为 1。 */
    uint8_t valid;
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint16_t steps;
    uint8_t average_heart_rate;
    int8_t temperature_c;
    uint8_t humidity_percent;
} History_Day_t;

void HistoryManager_Init(uint8_t eeprom_available);
/* 将刚结束的一天写入循环日志，并更新 RAM 镜像。 */
void HistoryManager_RecordDay(uint8_t year, uint8_t month, uint8_t day,
                              uint32_t steps, uint16_t heart_rate,
                              float temperature, float humidity);
uint8_t HistoryManager_GetCount(void);
/* index=0 取最新一天；成功返回 1，并把结果复制到 day。 */
uint8_t HistoryManager_GetNewest(uint8_t index, History_Day_t *day);

#endif
