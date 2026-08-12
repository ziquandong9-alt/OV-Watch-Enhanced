#include "ble_manager.h"

#include "KT6328.h"
#include "battery_manager.h"
#include "device_manager.h"
#include "notification_manager.h"
#include "rtc.h"
#include "usart.h"
#include "stm32f4xx_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLE_DMA_BUFFER_SIZE 64U
#define BLE_RING_SIZE       256U
#define BLE_LINE_SIZE       160U
#define BLE_LINK_TIMEOUT_MS 15000UL

static uint8_t s_dma_buffer[BLE_DMA_BUFFER_SIZE];
static volatile uint8_t s_ring[BLE_RING_SIZE];
static volatile uint16_t s_write_index;
static volatile uint16_t s_read_index;
static char s_line[BLE_LINE_SIZE];
static uint16_t s_line_length;
static uint8_t s_connected;
static uint8_t s_suspended;
static uint32_t s_last_packet_ms;
static uint32_t s_generation;

static uint8_t Weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t table[] = {0U, 3U, 2U, 5U, 0U, 3U,
                                    5U, 1U, 4U, 6U, 2U, 4U};
    uint32_t y = year;
    uint32_t value;
    if(month < 3U) y--;
    value = (y + y / 4U - y / 100U + y / 400U +
             table[month - 1U] + day) % 7U;
    return (value == 0U) ? RTC_WEEKDAY_SUNDAY : (uint8_t)value;
}

static void StartReceive(void)
{
    if(HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_dma_buffer,
                                   BLE_DMA_BUFFER_SIZE) == HAL_OK) {
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
}

static void SetConnected(uint8_t connected)
{
    connected = connected ? 1U : 0U;
    if(connected != s_connected) {
        s_connected = connected;
        s_generation++;
    }
}

static void ApplyTime(const char *payload)
{
    unsigned int year, month, day, hour, minute, second;
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    if(sscanf(payload, "%u,%u,%u,%u,%u,%u",
              &year, &month, &day, &hour, &minute, &second) != 6) return;
    if((year > 99U) || (month < 1U) || (month > 12U) ||
       (day < 1U) || (day > 31U) || (hour > 23U) ||
       (minute > 59U) || (second > 59U)) return;
    time.Hours = (uint8_t)hour;
    time.Minutes = (uint8_t)minute;
    time.Seconds = (uint8_t)second;
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;
    date.Year = (uint8_t)year;
    date.Month = (uint8_t)month;
    date.Date = (uint8_t)day;
    date.WeekDay = Weekday((uint16_t)(2000U + year),
                           (uint8_t)month, (uint8_t)day);
    if((HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) == HAL_OK) &&
       (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) == HAL_OK)) {
        DeviceManager_SaveDateTimeNow();
    }
}

static void HandleLine(char *line)
{
    char *separator;
    s_last_packet_ms = HAL_GetTick();
    SetConnected(1U);
    if((strcmp(line, "PING") == 0) || (strcmp(line, "CONNECTED") == 0)) return;
    if(strcmp(line, "DISCONNECTED") == 0) { SetConnected(0U); return; }
    if(strcmp(line, "CLEAR") == 0) { NotificationManager_ClearAll(); return; }
    if(strncmp(line, "TIME,", 5U) == 0) { ApplyTime(&line[5]); return; }
    if(strncmp(line, "NOTIFY,", 7U) == 0) {
        separator = strchr(&line[7], '|');
        if(separator != NULL) {
            *separator = '\0';
            NotificationManager_Push(NOTIFICATION_TYPE_BLE, &line[7], separator + 1);
        }
    }
}

void BLEManager_Init(void)
{
    s_write_index = 0U;
    s_read_index = 0U;
    s_line_length = 0U;
    s_connected = 0U;
    s_suspended = 0U;
    s_generation = 0U;
    KT6328_Init();
    StartReceive();
}

void BLEManager_Process(void)
{
    uint8_t byte;
    if(s_suspended != 0U) return;
    while(s_read_index != s_write_index) {
        byte = s_ring[s_read_index];
        s_read_index = (uint16_t)((s_read_index + 1U) % BLE_RING_SIZE);
        if((byte == '\n') || (byte == '\r')) {
            if(s_line_length != 0U) {
                s_line[s_line_length] = '\0';
                HandleLine(s_line);
                s_line_length = 0U;
            }
        }
        else if(s_line_length < (BLE_LINE_SIZE - 1U)) s_line[s_line_length++] = (char)byte;
        else s_line_length = 0U;
    }
    if((s_connected != 0U) &&
       ((uint32_t)(HAL_GetTick() - s_last_packet_ms) >= BLE_LINK_TIMEOUT_MS)) SetConnected(0U);
}

void BLEManager_Suspend(void)
{
    s_suspended = 1U;
    (void)HAL_UART_AbortReceive(&huart1);
    KT6328_Disable();
    SetConnected(0U);
}

void BLEManager_Resume(void)
{
    KT6328_Enable();
    s_suspended = 0U;
    StartReceive();
}

uint8_t BLEManager_IsConnected(void) { return s_connected; }
uint32_t BLEManager_GetGeneration(void) { return s_generation; }

void BLEManager_SendStatus(void)
{
    char message[96];
    const Battery_Data_t *battery = BatteryManager_Get();
    const Device_MotionData_t *motion = DeviceManager_GetMotion();
    int length = snprintf(message, sizeof(message), "STATUS,%u,%lu,%u\r\n",
                          battery->percent, motion->steps_today,
                          BLEManager_IsConnected());
    if((length > 0) && (length < (int)sizeof(message)))
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)message, (uint16_t)length, 20U);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *uart, uint16_t size)
{
    uint16_t i;
    uint16_t next;
    if(uart != &huart1) return;
    for(i = 0U; i < size; i++) {
        next = (uint16_t)((s_write_index + 1U) % BLE_RING_SIZE);
        if(next == s_read_index) break;
        s_ring[s_write_index] = s_dma_buffer[i];
        s_write_index = next;
    }
    if(s_suspended == 0U) StartReceive();
}
