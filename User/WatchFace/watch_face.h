#ifndef WATCH_FACE_H
#define WATCH_FACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the default analog watch face and start its RTC update timer.
 *
 * Call this once after lv_init(), lv_port_disp_init() and MX_RTC_Init().
 */
void WatchFace_Create(void);

/** Stop RTC updates and release every LVGL object owned by the watch face. */
void WatchFace_Destroy(void);

/** Switch between the normal animated face and the one-minute ambient face. */
void WatchFace_SetAmbientMode(uint8_t enabled);

/** Refresh the non-interactive red notification dot. */
void WatchFace_RefreshNotificationIndicator(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCH_FACE_H */
