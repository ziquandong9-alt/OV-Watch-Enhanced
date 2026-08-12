#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <stdint.h>

#define NOTIFICATION_MAX_COUNT       12U
#define NOTIFICATION_TITLE_LENGTH    28U
#define NOTIFICATION_MESSAGE_LENGTH  112U

typedef enum {
    NOTIFICATION_TYPE_HEART = 0,
    NOTIFICATION_TYPE_ENVIRONMENT,
    NOTIFICATION_TYPE_SEDENTARY,
    NOTIFICATION_TYPE_FALL,
    NOTIFICATION_TYPE_BATTERY,
    NOTIFICATION_TYPE_BLE
} Notification_Type_t;

typedef struct {
    uint32_t id;
    Notification_Type_t type;
    uint8_t hour;
    uint8_t minute;
    char title[NOTIFICATION_TITLE_LENGTH];
    char message[NOTIFICATION_MESSAGE_LENGTH];
} Notification_Record_t;

void NotificationManager_Init(void);
uint8_t NotificationManager_Push(Notification_Type_t type,
                                 const char *title,
                                 const char *message);
uint8_t NotificationManager_GetCount(void);
const Notification_Record_t *NotificationManager_Get(uint8_t index);
uint8_t NotificationManager_Remove(uint32_t id);
void NotificationManager_ClearAll(void);
uint32_t NotificationManager_GetGeneration(void);

#endif
