#ifndef HISTORY_MANAGER_H
#define HISTORY_MANAGER_H

#include <stdint.h>

#define HISTORY_MAX_DAYS 7U

typedef struct {
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
void HistoryManager_RecordDay(uint8_t year, uint8_t month, uint8_t day,
                              uint32_t steps, uint16_t heart_rate,
                              float temperature, float humidity);
uint8_t HistoryManager_GetCount(void);
uint8_t HistoryManager_GetNewest(uint8_t index, History_Day_t *day);

#endif
