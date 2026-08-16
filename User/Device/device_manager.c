#include "device_manager.h"

#include "AHT21.h"
#include "BL24C02.h"
#include "DataSave.h"
#include "LSM303.h"
#include "em70x8.h"
#include "mpu6050.h"
#include "notification_manager.h"
#include "history_manager.h"
#include "rtc.h"
#include "stm32f4xx_hal.h"

#include <string.h>

#define ENVIRONMENT_PERIOD_MS       (20UL * 60UL * 1000UL)
#define ENVIRONMENT_LIVE_PERIOD_MS  1000UL
#define ENVIRONMENT_CONVERT_MS      85UL
#define HEART_PERIOD_MS             (15UL * 60UL * 1000UL)
#define HEART_SAMPLE_PERIOD_MS      50UL
#define HEART_MEASURE_DURATION_MS   12000UL
#define EEPROM_STEP_SAVE_PERIOD_MS  (30UL * 60UL * 1000UL)
#define WRIST_SAMPLE_PERIOD_MS      300UL
#define STEP_SAMPLE_PERIOD_MS       100UL
#define STEP_DATE_CHECK_PERIOD_MS   1000UL
#define STEP_MIN_INTERVAL_MS        280UL
#define STEP_DYNAMIC_HIGH           650L
#define STEP_DYNAMIC_LOW            260L
#define ACTIVITY_DYNAMIC_THRESHOLD  500L
#define HEART_ALERT_COOLDOWN_MS     (2UL * 60UL * 60UL * 1000UL)
#define ENVIRONMENT_EXPOSURE_MS     (45UL * 60UL * 1000UL)
#define ENVIRONMENT_ALERT_COOLDOWN_MS (4UL * 60UL * 60UL * 1000UL)
#define SEDENTARY_ALERT_MS          (45UL * 60UL * 1000UL)
#define FALL_IMPACT_MAGNITUDE       10000L
#define FALL_CONFIRM_DELAY_MS       1200UL
#define FALL_CANCEL_MS              3500UL
#define FALL_STILL_DYNAMIC          360L
#define FALL_ALERT_COOLDOWN_MS      (10UL * 60UL * 1000UL)

#define EEPROM_LEGACY_TIME_ADDRESS      0x08U
#define EEPROM_LEGACY_SETTINGS_ADDRESS  0x10U
#define EEPROM_LEGACY_STEPS_ADDRESS     0x18U
#define EEPROM_SETTINGS_BASE        0x20U
#define EEPROM_SETTINGS_SLOTS       4U
#define EEPROM_TIME_BASE            0x40U
#define EEPROM_TIME_SLOTS           3U
#define EEPROM_TIME_SLOT_SIZE       16U
#define EEPROM_STEPS_BASE           0x70U
#define EEPROM_STEPS_SLOTS          4U
#define EEPROM_RECORD_SIZE          8U
#define EEPROM_TIME_MARKER          0xA5U
#define EEPROM_SETTINGS_MARKER      0x5AU
#define EEPROM_STEPS_MARKER         0xC7U
#define EEPROM_SETTINGS_VERSION     2U
#define DEFAULT_WORK_BRIGHTNESS     50U
#define DEFAULT_AMBIENT_BRIGHTNESS  5U

static Device_EnvironmentData_t s_environment;
static Device_HeartData_t s_heart;
static Device_MotionData_t s_motion;
static Device_CompassData_t s_compass;
static uint8_t s_eeprom_connected;
static uint8_t s_wrist_wake_enabled;
static uint8_t s_ambient_enabled = 1U;
static uint8_t s_working_brightness = DEFAULT_WORK_BRIGHTNESS;
static uint8_t s_ambient_brightness = DEFAULT_AMBIENT_BRIGHTNESS;
static uint8_t s_watch_face;
static uint8_t s_wrist_was_up;
static uint8_t s_wrist_raise_event;
static uint8_t s_wrist_lower_event;
static uint8_t s_compass_open;
static uint8_t s_environment_open;
static uint8_t s_environment_converting;
static uint32_t s_environment_ready_ms;
static uint32_t s_next_step_save_ms;
static uint32_t s_last_saved_steps;
static uint8_t s_settings_next_slot;
static uint8_t s_settings_next_sequence;
static uint8_t s_time_next_slot;
static uint8_t s_time_next_sequence;
static uint8_t s_steps_next_slot;
static uint8_t s_steps_next_sequence;
static uint32_t s_next_wrist_sample_ms;
static uint32_t s_next_step_sample_ms;
static uint32_t s_next_step_date_check_ms;
static uint32_t s_step_last_ms;
static int32_t s_step_magnitude_baseline;
static uint8_t s_step_peak_armed;
static uint8_t s_step_year;
static uint8_t s_step_month;
static uint8_t s_step_day;
static uint32_t s_heart_started_ms;
static uint32_t s_next_heart_sample_ms;
static uint32_t s_heart_dc;
static int32_t s_heart_previous_ac;
static uint32_t s_heart_last_peak_ms;
static uint16_t s_heart_bpm_sum;
static uint8_t s_heart_bpm_count;
static int8_t s_heart_abnormal_kind;
static uint8_t s_heart_abnormal_count;
static uint32_t s_heart_last_alert_ms;
static uint8_t s_environment_exposure_active;
static uint8_t s_environment_exposure_kind;
static uint32_t s_environment_exposure_started_ms;
static uint32_t s_environment_last_alert_ms;
static uint32_t s_last_activity_ms;
static uint8_t s_sedentary_notified;
static uint8_t s_fall_candidate;
static uint8_t s_fall_still_samples;
static uint32_t s_fall_impact_ms;
static uint32_t s_fall_last_alert_ms;
static uint32_t s_daily_heart_sum;
static uint16_t s_daily_heart_count;
static float s_daily_temperature_sum;
static float s_daily_humidity_sum;
static uint16_t s_daily_environment_count;

static uint8_t IsDue(uint32_t now, uint32_t deadline);
static uint8_t Checksum(const uint8_t *data);
static uint8_t SequenceIsNewer(uint8_t candidate, uint8_t current);
static uint8_t FindNewest8(uint8_t base, uint8_t slots, uint8_t marker,
                           uint8_t *record, uint8_t *newest_slot);
static uint8_t Weekday(uint16_t year, uint8_t month, uint8_t day);
static void LoadSettings(void);
static void SaveSettings(void);
static void LoadDateTime(void);
static void LoadSteps(void);
static void SaveSteps(void);
static void ProcessHeart(uint32_t now);
static void ProcessEnvironment(uint32_t now);
static void ProcessWrist(uint32_t now);
static void ProcessSteps(uint32_t now);
static void ProcessStepDate(uint32_t now);
static void ProcessSedentary(uint32_t now);
static void ProcessFall(uint32_t now, int32_t magnitude, int32_t dynamic);
static void EvaluateHeartAlert(uint32_t now);
static void EvaluateEnvironmentAlert(uint32_t now,
                                     float temperature,
                                     float humidity);
static void RecordCompletedDay(void);

void DeviceManager_Init(void)
{
    uint32_t now = HAL_GetTick();

    memset(&s_environment, 0, sizeof(s_environment));
    memset(&s_heart, 0, sizeof(s_heart));
    memset(&s_motion, 0, sizeof(s_motion));
    memset(&s_compass, 0, sizeof(s_compass));

    EEPROM_Init();
    s_eeprom_connected = (EEPROM_Check() == 0U) ? 1U : 0U;
    if(s_eeprom_connected != 0U) {
        LoadSettings();
        LoadDateTime();
        HistoryManager_Init(1U);
        LoadSteps();
    }
    else HistoryManager_Init(0U);

    s_motion.connected = (MPU_Init() == 0U) ? 1U : 0U;
    if(s_motion.connected != 0U) {
        s_wrist_was_up = MPU_isHorizontal();
        /* Keep only the low-power accelerometer alive for step counting. */
        MPU_Wakeup();
        if(s_wrist_wake_enabled != 0U) MPU_Motion_Init();
        else (void)MPU_Write_Byte(MPU_INT_EN_REG, 0x00U);
    }
    s_environment.connected = (AHT_Init() == 0U) ? 1U : 0U;
    s_heart.connected = (EM7028_hrs_init() == 0U) ? 1U : 0U;
    s_compass.connected = (LSM303DLH_Init() == 0U) ? 1U : 0U;
    if(s_compass.connected != 0U) {
        LSM303DLH_Sleep();
    }

    s_next_step_save_ms = now + EEPROM_STEP_SAVE_PERIOD_MS;
    s_next_wrist_sample_ms = now + WRIST_SAMPLE_PERIOD_MS;
    s_next_step_sample_ms = now;
    s_next_step_date_check_ms = now;
    s_step_peak_armed = 1U;
    s_last_activity_ms = now;
    s_sedentary_notified = 0U;
    s_fall_candidate = 0U;
    s_heart_abnormal_count = 0U;
    s_environment_exposure_active = 0U;
    DeviceManager_ForceEnvironmentUpdate();
    DeviceManager_StartHeartMeasurement();
}

void DeviceManager_Process(void)
{
    uint32_t now = HAL_GetTick();

    ProcessEnvironment(now);
    ProcessSteps(now);
    ProcessStepDate(now);
    ProcessSedentary(now);
    if((s_heart.measuring == 0U) &&
       (IsDue(now, s_heart.next_update_ms) != 0U)) {
        DeviceManager_StartHeartMeasurement();
    }
    ProcessHeart(now);
    ProcessWrist(now);

    if(IsDue(now, s_next_step_save_ms) != 0U) {
        if(s_motion.steps_today != s_last_saved_steps) SaveSteps();
        s_next_step_save_ms = now + EEPROM_STEP_SAVE_PERIOD_MS;
    }
}

static void ProcessEnvironment(uint32_t now)
{
    float humidity;
    float temperature;

    if(s_environment.connected == 0U) return;

    if(s_environment_converting != 0U) {
        if(IsDue(now, s_environment_ready_ms) == 0U) return;
        s_environment_converting = 0U;
        if((AHT_ReadMeasurement(&humidity, &temperature) == 0U) &&
           (temperature > -40.0f) && (temperature < 85.0f) &&
           (humidity >= 0.0f) && (humidity <= 100.0f)) {
            s_environment.temperature = temperature;
            s_environment.humidity = humidity;
            s_environment.updated_at_ms = now;
            s_daily_temperature_sum += temperature;
            s_daily_humidity_sum += humidity;
            if(s_daily_environment_count < 65535U) s_daily_environment_count++;
            EvaluateEnvironmentAlert(now, temperature, humidity);
        }
        s_environment.next_update_ms = now +
            ((s_environment_open != 0U) ? ENVIRONMENT_LIVE_PERIOD_MS :
                                         ENVIRONMENT_PERIOD_MS);
        return;
    }

    if(IsDue(now, s_environment.next_update_ms) != 0U) {
        AHT_StartMeasurement();
        s_environment_converting = 1U;
        s_environment_ready_ms = now + ENVIRONMENT_CONVERT_MS;
    }
}

static void EvaluateEnvironmentAlert(uint32_t now,
                                     float temperature,
                                     float humidity)
{
    uint8_t kind = 0U;
    const char *title = "Environment alert";
    const char *message = NULL;

    if(temperature >= 35.0f) kind = 1U;
    else if(temperature <= 5.0f) kind = 2U;
    else if(humidity >= 85.0f) kind = 3U;
    else if(humidity <= 20.0f) kind = 4U;

    if(kind == 0U) {
        s_environment_exposure_active = 0U;
        s_environment_exposure_kind = 0U;
        return;
    }

    if((s_environment_exposure_active == 0U) ||
       (kind != s_environment_exposure_kind)) {
        s_environment_exposure_active = 1U;
        s_environment_exposure_kind = kind;
        s_environment_exposure_started_ms = now;
        return;
    }

    if((uint32_t)(now - s_environment_exposure_started_ms) <
       ENVIRONMENT_EXPOSURE_MS) return;
    if((s_environment_last_alert_ms != 0U) &&
       ((uint32_t)(now - s_environment_last_alert_ms) <
        ENVIRONMENT_ALERT_COOLDOWN_MS)) return;

    switch(kind) {
    case 1U:
        message = "High temperature has persisted. Move somewhere cooler and drink water.";
        break;
    case 2U:
        message = "Low temperature has persisted. Move somewhere warmer and protect exposed skin.";
        break;
    case 3U:
        message = "Very high humidity has persisted. Ventilate or move to a drier place.";
        break;
    case 4U:
        message = "Very dry air has persisted. Drink water and consider adding humidity.";
        break;
    default:
        break;
    }

    if((message != NULL) &&
       (NotificationManager_Push(NOTIFICATION_TYPE_ENVIRONMENT,
                                 title, message) != 0U)) {
        s_environment_last_alert_ms = now;
    }
}

uint8_t DeviceManager_CanEnterStop(void)
{
    return (s_heart.measuring == 0U) ? 1U : 0U;
}

void DeviceManager_ForceEnvironmentUpdate(void)
{
    uint32_t now = HAL_GetTick();
    float humidity;
    float temperature;

    if((s_environment.connected != 0U) &&
       (AHT_Read(&humidity, &temperature) == 0U) &&
       (temperature > -40.0f) && (temperature < 85.0f) &&
       (humidity >= 0.0f) && (humidity <= 100.0f)) {
        s_environment.temperature = temperature;
        s_environment.humidity = humidity;
        s_environment.updated_at_ms = now;
        s_daily_temperature_sum += temperature;
        s_daily_humidity_sum += humidity;
        if(s_daily_environment_count < 65535U) s_daily_environment_count++;
        EvaluateEnvironmentAlert(now, temperature, humidity);
    }
    s_environment.next_update_ms = now + ENVIRONMENT_PERIOD_MS;
}

void DeviceManager_EnvironmentOpen(void)
{
    s_environment_open = 1U;
    s_environment.next_update_ms = HAL_GetTick();
}

void DeviceManager_EnvironmentClose(void)
{
    s_environment_open = 0U;
    /* Finish an in-flight conversion, then resume the 20-minute schedule. */
    if(s_environment_converting == 0U) {
        s_environment.next_update_ms = HAL_GetTick() + ENVIRONMENT_PERIOD_MS;
    }
}

void DeviceManager_StartHeartMeasurement(void)
{
    uint32_t now = HAL_GetTick();

    s_heart.next_update_ms = now + HEART_PERIOD_MS;
    if(s_heart.connected == 0U) {
        /* Re-probe on every manual entry in case EM7028 powered up late. */
        s_heart.connected = (EM7028_hrs_init() == 0U) ? 1U : 0U;
        if(s_heart.connected == 0U) {
            s_heart.measuring = 0U;
            return;
        }
    }
    if(EM7028_hrs_Enable() != 0U) {
        s_heart.connected = 0U;
        s_heart.measuring = 0U;
        return;
    }

    s_heart.measuring = 1U;
    s_heart_started_ms = now;
    s_next_heart_sample_ms = now;
    s_heart_dc = 0U;
    s_heart_previous_ac = 0;
    s_heart_last_peak_ms = 0U;
    s_heart_bpm_sum = 0U;
    s_heart_bpm_count = 0U;
}

void DeviceManager_StopHeartMeasurement(void)
{
    if(s_heart.measuring != 0U) {
        (void)EM7028_hrs_DisEnable();
        s_heart.measuring = 0U;
        s_heart.updated_at_ms = HAL_GetTick();
    }
}

static void ProcessHeart(uint32_t now)
{
    uint16_t raw;
    int32_t ac;
    uint32_t interval;
    uint16_t bpm;

    if(s_heart.measuring == 0U) return;

    if((uint32_t)(now - s_heart_started_ms) >= HEART_MEASURE_DURATION_MS) {
        (void)EM7028_hrs_DisEnable();
        s_heart.measuring = 0U;
        s_heart.updated_at_ms = now;
        if((s_heart_bpm_count != 0U) && (s_heart_bpm_count <= 4U)) {
            s_heart.bpm = s_heart_bpm_sum / s_heart_bpm_count;
        }
        if((s_heart_bpm_count != 0U) && (s_heart.bpm != 0U)) {
            s_daily_heart_sum += s_heart.bpm;
            if(s_daily_heart_count < 65535U) s_daily_heart_count++;
        }
        EvaluateHeartAlert(now);
        return;
    }
    if(IsDue(now, s_next_heart_sample_ms) == 0U) return;
    s_next_heart_sample_ms = now + HEART_SAMPLE_PERIOD_MS;

    raw = EM7028_Get_HRS1();
    s_heart.raw = raw;
    s_heart_dc = (s_heart_dc == 0U) ? raw :
                 (((s_heart_dc * 31U) + raw) / 32U);
    ac = (int32_t)raw - (int32_t)s_heart_dc;

    if((s_heart_previous_ac <= 120) && (ac > 120)) {
        if(s_heart_last_peak_ms != 0U) {
            interval = now - s_heart_last_peak_ms;
            if((interval >= 333U) && (interval <= 1500U)) {
                bpm = (uint16_t)(60000U / interval);
                if((bpm >= 40U) && (bpm <= 180U)) {
                    if(s_heart_bpm_count < 4U) {
                        s_heart_bpm_sum += bpm;
                        s_heart_bpm_count++;
                    }
                    if(s_heart_bpm_count < 4U) {
                        s_heart.bpm = s_heart_bpm_sum / s_heart_bpm_count;
                    }
                    else if(s_heart_bpm_count == 4U) {
                        s_heart.bpm = s_heart_bpm_sum / 4U;
                        s_heart_bpm_count = 5U;
                    }
                    else {
                        s_heart.bpm = (uint16_t)
                            (((uint32_t)s_heart.bpm * 3U + bpm) / 4U);
                    }
                }
            }
        }
        s_heart_last_peak_ms = now;
    }
    s_heart_previous_ac = ac;
}

static void EvaluateHeartAlert(uint32_t now)
{
    int8_t abnormal_kind = 0;
    const char *message;

    if(s_heart_bpm_count == 0U) return;
    if((s_heart.bpm >= 40U) && (s_heart.bpm < 50U)) abnormal_kind = -1;
    else if((s_heart.bpm > 110U) && (s_heart.bpm <= 180U)) abnormal_kind = 1;

    if(abnormal_kind == 0) {
        s_heart_abnormal_kind = 0;
        s_heart_abnormal_count = 0U;
        return;
    }

    if(abnormal_kind == s_heart_abnormal_kind) {
        if(s_heart_abnormal_count < 255U) s_heart_abnormal_count++;
    }
    else {
        s_heart_abnormal_kind = abnormal_kind;
        s_heart_abnormal_count = 1U;
    }

    if((s_heart_abnormal_count >= 2U) &&
       ((s_heart_last_alert_ms == 0U) ||
        ((uint32_t)(now - s_heart_last_alert_ms) >= HEART_ALERT_COOLDOWN_MS))) {
        message = (abnormal_kind > 0) ?
            "Repeated high heart-rate readings. Rest and recheck; seek help if you feel unwell." :
            "Repeated low heart-rate readings. Rest and recheck; seek help if you feel unwell.";
        if(NotificationManager_Push(NOTIFICATION_TYPE_HEART,
                                    "Heart rate alert", message) != 0U) {
            s_heart_last_alert_ms = now;
        }
    }
}

void DeviceManager_UpdateMotion(void)
{
    s_next_step_sample_ms = HAL_GetTick();
    ProcessSteps(HAL_GetTick());
}

void DeviceManager_MotionOpen(void)
{
    /* The activity page reads the always-running low-power step counter. */
    DeviceManager_UpdateMotion();
}

void DeviceManager_MotionClose(void)
{
    /* Step counting must continue after leaving the page. */
}

static void ProcessSteps(uint32_t now)
{
    short x;
    short y;
    short z;
    int32_t ax;
    int32_t ay;
    int32_t az;
    int32_t largest;
    int32_t middle;
    int32_t smallest;
    int32_t temporary;
    int32_t magnitude;
    int32_t dynamic;

    if((s_motion.connected == 0U) ||
       (IsDue(now, s_next_step_sample_ms) == 0U)) return;
    s_next_step_sample_ms = now + STEP_SAMPLE_PERIOD_MS;

    if(MPU_Get_Accelerometer(&x, &y, &z) != 0U) return;
    s_motion.accel_x = x;
    s_motion.accel_y = y;
    s_motion.accel_z = z;

    ax = (x < 0) ? -(int32_t)x : (int32_t)x;
    ay = (y < 0) ? -(int32_t)y : (int32_t)y;
    az = (z < 0) ? -(int32_t)z : (int32_t)z;
    largest = ax;
    middle = ay;
    smallest = az;
    if(largest < middle) { temporary = largest; largest = middle; middle = temporary; }
    if(middle < smallest) { temporary = middle; middle = smallest; smallest = temporary; }
    if(largest < middle) { temporary = largest; largest = middle; middle = temporary; }

    /* Cheap orientation-independent approximation of sqrt(x*x+y*y+z*z). */
    magnitude = largest + (middle >> 1) + (smallest >> 2);
    if(s_step_magnitude_baseline == 0L) {
        s_step_magnitude_baseline = magnitude;
        return;
    }

    dynamic = magnitude - s_step_magnitude_baseline;
    if(dynamic < 0L) dynamic = -dynamic;
    s_step_magnitude_baseline +=
        (magnitude - s_step_magnitude_baseline) / 16L;

    if(dynamic >= ACTIVITY_DYNAMIC_THRESHOLD) {
        s_last_activity_ms = now;
        s_sedentary_notified = 0U;
    }
    ProcessFall(now, magnitude, dynamic);

    if((s_step_peak_armed != 0U) &&
       (dynamic >= STEP_DYNAMIC_HIGH) &&
       ((uint32_t)(now - s_step_last_ms) >= STEP_MIN_INTERVAL_MS)) {
        s_motion.steps_today++;
        s_motion.updated_at_ms = now;
        s_step_last_ms = now;
        s_step_peak_armed = 0U;
    }
    else if(dynamic <= STEP_DYNAMIC_LOW) {
        s_step_peak_armed = 1U;
    }
}

static void ProcessFall(uint32_t now, int32_t magnitude, int32_t dynamic)
{
    uint32_t elapsed;

    if(magnitude >= FALL_IMPACT_MAGNITUDE) {
        s_fall_candidate = 1U;
        s_fall_impact_ms = now;
        s_fall_still_samples = 0U;
        return;
    }
    if(s_fall_candidate == 0U) return;

    elapsed = now - s_fall_impact_ms;
    if(elapsed > FALL_CANCEL_MS) {
        s_fall_candidate = 0U;
        s_fall_still_samples = 0U;
        return;
    }

    if(elapsed >= (FALL_CONFIRM_DELAY_MS / 2U)) {
        if(dynamic <= FALL_STILL_DYNAMIC) {
            if(s_fall_still_samples < 255U) s_fall_still_samples++;
        }
        else if(dynamic >= ACTIVITY_DYNAMIC_THRESHOLD) {
            s_fall_still_samples = 0U;
        }
    }

    if((elapsed >= FALL_CONFIRM_DELAY_MS) &&
       (s_fall_still_samples >= 4U)) {
        if((s_fall_last_alert_ms == 0U) ||
           ((uint32_t)(now - s_fall_last_alert_ms) >=
            FALL_ALERT_COOLDOWN_MS)) {
            if(NotificationManager_Push(
                   NOTIFICATION_TYPE_FALL,
                   "Possible fall detected",
                   "A hard impact followed by stillness was detected. Check yourself and contact help if needed.") != 0U) {
                s_fall_last_alert_ms = now;
            }
        }
        s_fall_candidate = 0U;
        s_fall_still_samples = 0U;
    }
}

static void ProcessSedentary(uint32_t now)
{
    if((s_motion.connected == 0U) || (s_sedentary_notified != 0U)) return;
    if((uint32_t)(now - s_last_activity_ms) < SEDENTARY_ALERT_MS) return;

    if(NotificationManager_Push(
           NOTIFICATION_TYPE_SEDENTARY,
           "Time to move",
           "You have been inactive for 45 minutes. Stand up, stretch, and drink some water.") != 0U) {
        s_sedentary_notified = 1U;
    }
}

static void ProcessStepDate(uint32_t now)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    if(IsDue(now, s_next_step_date_check_ms) == 0U) return;
    s_next_step_date_check_ms = now + STEP_DATE_CHECK_PERIOD_MS;
    if(HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) return;
    if(HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) return;

    if((date.Year != s_step_year) ||
       (date.Month != s_step_month) ||
       (date.Date != s_step_day)) {
        RecordCompletedDay();
        s_step_year = date.Year;
        s_step_month = date.Month;
        s_step_day = date.Date;
        s_motion.steps_today = 0U;
        s_motion.updated_at_ms = now;
        SaveSteps();
    }
}

void DeviceManager_CompassOpen(void)
{
    s_compass_open = 1U;
    if(s_compass.connected != 0U) {
        LSM303DLH_Wakeup();
        DeviceManager_UpdateCompass();
    }
}

void DeviceManager_UpdateCompass(void)
{
    int16_t xa, ya, za, xm, ym, zm;
    float direction;

    if((s_compass.connected == 0U) || (s_compass_open == 0U)) return;
    LSM303_ReadAcceleration(&xa, &ya, &za);
    LSM303_ReadMagnetic(&xm, &ym, &zm);
    direction = Azimuth_Calculate(xa, ya, za, xm, ym, zm);
    if(direction < 0.0f) direction += 360.0f;
    if((direction >= 0.0f) && (direction <= 360.0f)) {
        s_compass.direction_deg = (uint16_t)direction;
    }
}

void DeviceManager_CompassClose(void)
{
    s_compass_open = 0U;
    if(s_compass.connected != 0U) LSM303DLH_Sleep();
}

static void ProcessWrist(uint32_t now)
{
    uint8_t wrist_up;

    if((s_wrist_wake_enabled == 0U) || (s_motion.connected == 0U) ||
       (IsDue(now, s_next_wrist_sample_ms) == 0U)) return;

    s_next_wrist_sample_ms = now + WRIST_SAMPLE_PERIOD_MS;
    wrist_up = MPU_isHorizontal();
    if((wrist_up != 0U) && (s_wrist_was_up == 0U)) {
        s_wrist_raise_event = 1U;
    }
    else if((wrist_up == 0U) && (s_wrist_was_up != 0U)) {
        s_wrist_lower_event = 1U;
    }
    s_wrist_was_up = wrist_up;
}

uint8_t DeviceManager_TakeWristRaiseEvent(void)
{
    uint8_t event = s_wrist_raise_event;
    s_wrist_raise_event = 0U;
    return event;
}

uint8_t DeviceManager_TakeWristLowerEvent(void)
{
    uint8_t event = s_wrist_lower_event;
    s_wrist_lower_event = 0U;
    return event;
}

uint8_t DeviceManager_CheckWristAfterStop(void)
{
    uint8_t wrist_up;
    if((s_wrist_wake_enabled == 0U) || (s_motion.connected == 0U)) return 0U;
    wrist_up = MPU_isHorizontal();
    if((wrist_up != 0U) && (s_wrist_was_up == 0U)) {
        s_wrist_was_up = wrist_up;
        return 1U;
    }
    s_wrist_was_up = wrist_up;
    return 0U;
}

const Device_EnvironmentData_t *DeviceManager_GetEnvironment(void) { return &s_environment; }
const Device_HeartData_t *DeviceManager_GetHeart(void) { return &s_heart; }
const Device_MotionData_t *DeviceManager_GetMotion(void) { return &s_motion; }
const Device_CompassData_t *DeviceManager_GetCompass(void) { return &s_compass; }

uint8_t DeviceManager_GetWristWakeEnabled(void) { return s_wrist_wake_enabled; }
void DeviceManager_SetWristWakeEnabled(uint8_t enabled)
{
    uint8_t value = (enabled != 0U) ? 1U : 0U;
    if(value == s_wrist_wake_enabled) return;
    s_wrist_wake_enabled = value;
    s_wrist_raise_event = 0U;
    s_wrist_lower_event = 0U;
    if(s_motion.connected != 0U) {
        if(s_wrist_wake_enabled != 0U) {
            MPU_Wakeup();
            MPU_Motion_Init();
            s_wrist_was_up = MPU_isHorizontal();
        }
        else {
            /* Keep low-power acceleration sampling for the step counter. */
            MPU_Wakeup();
            (void)MPU_Write_Byte(MPU_INT_EN_REG, 0x00U);
        }
    }
    SaveSettings();
}

uint8_t DeviceManager_GetAmbientEnabled(void) { return s_ambient_enabled; }
void DeviceManager_SetAmbientEnabled(uint8_t enabled)
{
    uint8_t value = (enabled != 0U) ? 1U : 0U;
    if(value == s_ambient_enabled) return;
    s_ambient_enabled = value;
    SaveSettings();
}

uint8_t DeviceManager_GetWatchFace(void)
{
    return s_watch_face;
}

void DeviceManager_SetWatchFace(uint8_t index)
{
    uint8_t value = (index <= 2U) ? index : 0U;
    if(value == s_watch_face) return;
    s_watch_face = value;
    SaveSettings();
}

uint8_t DeviceManager_GetWorkingBrightness(void)
{
    return s_working_brightness;
}

uint8_t DeviceManager_GetAmbientBrightness(void)
{
    return s_ambient_brightness;
}

void DeviceManager_SetBrightness(uint8_t working_percent,
                                 uint8_t ambient_percent)
{
    if(working_percent < 1U) working_percent = 1U;
    if(working_percent > 100U) working_percent = 100U;
    if(ambient_percent < 1U) ambient_percent = 1U;
    if(ambient_percent > working_percent) ambient_percent = working_percent;
    if((working_percent == s_working_brightness) &&
       (ambient_percent == s_ambient_brightness)) return;
    s_working_brightness = working_percent;
    s_ambient_brightness = ambient_percent;
    SaveSettings();
}

void DeviceManager_SaveDateTimeNow(void)
{
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};
    uint8_t record[EEPROM_TIME_SLOT_SIZE] = {0};
    uint8_t address;

    if(s_eeprom_connected == 0U) return;
    if(HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) return;
    if(HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) return;

    if((date.Year != s_step_year) ||
       (date.Month != s_step_month) ||
       (date.Date != s_step_day)) {
        RecordCompletedDay();
        s_step_year = date.Year;
        s_step_month = date.Month;
        s_step_day = date.Date;
        s_motion.steps_today = 0U;
    }

    record[0] = EEPROM_TIME_MARKER;
    record[1] = s_time_next_sequence;
    record[2] = date.Year;
    record[3] = date.Month;
    record[4] = date.Date;
    record[5] = time.Hours;
    record[6] = time.Minutes;
    record[7] = Checksum(record);
    record[8] = (uint8_t)~EEPROM_TIME_MARKER;
    record[9] = s_time_next_sequence;
    record[10] = time.Seconds;
    record[15] = Checksum(&record[8]);
    address = (uint8_t)(EEPROM_TIME_BASE +
                        s_time_next_slot * EEPROM_TIME_SLOT_SIZE);
    BL24C02_Write((uint8_t)(address + 8U), 8U, &record[8]);
    HAL_Delay(6U);
    BL24C02_Write(address, 8U, record);
    HAL_Delay(6U);
    s_time_next_slot = (uint8_t)((s_time_next_slot + 1U) % EEPROM_TIME_SLOTS);
    s_time_next_sequence++;
    SaveSteps();
}

static void LoadDateTime(void)
{
    uint8_t record[EEPROM_TIME_SLOT_SIZE];
    uint8_t candidate[EEPROM_TIME_SLOT_SIZE];
    uint8_t slot;
    uint8_t newest_slot = 0U;
    uint8_t newest_sequence = 0U;
    uint8_t found = 0U;
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    for(slot = 0U; slot < EEPROM_TIME_SLOTS; slot++) {
        BL24C02_Read((uint8_t)(EEPROM_TIME_BASE + slot * EEPROM_TIME_SLOT_SIZE),
                     EEPROM_TIME_SLOT_SIZE, candidate);
        if((candidate[0] != EEPROM_TIME_MARKER) ||
           (candidate[8] != (uint8_t)~EEPROM_TIME_MARKER) ||
           (candidate[1] != candidate[9]) ||
           (candidate[7] != Checksum(candidate)) ||
           (candidate[15] != Checksum(&candidate[8])) ||
           (candidate[3] < 1U) || (candidate[3] > 12U) ||
           (candidate[4] < 1U) || (candidate[4] > 31U) ||
           (candidate[5] > 23U) || (candidate[6] > 59U) ||
           (candidate[10] > 59U)) continue;
        if((found == 0U) || SequenceIsNewer(candidate[1], newest_sequence)) {
            memcpy(record, candidate, sizeof(record));
            newest_sequence = candidate[1];
            newest_slot = slot;
            found = 1U;
        }
    }
    if(found != 0U) {
        s_time_next_slot = (uint8_t)((newest_slot + 1U) % EEPROM_TIME_SLOTS);
        s_time_next_sequence = (uint8_t)(newest_sequence + 1U);
    }
    else {
        /* One-time migration from the original fixed-address record. */
        BL24C02_Read(EEPROM_LEGACY_TIME_ADDRESS, EEPROM_RECORD_SIZE, record);
        if((record[0] != EEPROM_TIME_MARKER) ||
           (record[7] != Checksum(record)) ||
           (record[2] < 1U) || (record[2] > 12U) ||
           (record[3] < 1U) || (record[3] > 31U) ||
           (record[4] > 23U) || (record[5] > 59U) ||
           (record[6] > 59U)) return;
        time.Hours = record[4]; time.Minutes = record[5]; time.Seconds = record[6];
        date.Year = record[1]; date.Month = record[2]; date.Date = record[3];
        goto apply_time;
    }

    time.Hours = record[5];
    time.Minutes = record[6];
    time.Seconds = record[10];
    date.Year = record[2];
    date.Month = record[3];
    date.Date = record[4];
apply_time:
    time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    time.StoreOperation = RTC_STOREOPERATION_RESET;
    date.WeekDay = Weekday(2000U + date.Year, date.Month, date.Date);
    (void)HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
    (void)HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
}

static void LoadSettings(void)
{
    uint8_t record[EEPROM_RECORD_SIZE];
    uint8_t newest_slot;

    if(FindNewest8(EEPROM_SETTINGS_BASE, EEPROM_SETTINGS_SLOTS,
                   EEPROM_SETTINGS_MARKER, record, &newest_slot) != 0U) {
        s_settings_next_slot = (uint8_t)((newest_slot + 1U) % EEPROM_SETTINGS_SLOTS);
        s_settings_next_sequence = (uint8_t)(record[1] + 1U);
        if(record[2] != EEPROM_SETTINGS_VERSION) return;
        s_wrist_wake_enabled = (record[3] & 0x01U) ? 1U : 0U;
        s_ambient_enabled = (record[3] & 0x02U) ? 1U : 0U;
        if((record[4] >= 1U) && (record[4] <= 100U)) s_working_brightness = record[4];
        if((record[5] >= 1U) && (record[5] <= s_working_brightness)) s_ambient_brightness = record[5];
        if(record[6] <= 2U) s_watch_face = record[6];
        return;
    }

    BL24C02_Read(EEPROM_LEGACY_SETTINGS_ADDRESS, EEPROM_RECORD_SIZE, record);
    if((record[0] != EEPROM_SETTINGS_MARKER) ||
       (record[7] != Checksum(record)) ||
       ((record[1] != 1U) && (record[1] != EEPROM_SETTINGS_VERSION))) return;
    s_wrist_wake_enabled = (record[2] != 0U) ? 1U : 0U;
    s_ambient_enabled = (record[3] != 0U) ? 1U : 0U;
    if((record[4] >= 1U) && (record[4] <= 100U)) s_working_brightness = record[4];
    if((record[5] >= 1U) && (record[5] <= s_working_brightness)) s_ambient_brightness = record[5];
}

static void SaveSettings(void)
{
    uint8_t record[EEPROM_RECORD_SIZE] = {0};

    if(s_eeprom_connected == 0U) return;
    record[0] = EEPROM_SETTINGS_MARKER;
    record[1] = s_settings_next_sequence;
    record[2] = EEPROM_SETTINGS_VERSION;
    record[3] = (uint8_t)((s_wrist_wake_enabled ? 0x01U : 0U) |
                          (s_ambient_enabled ? 0x02U : 0U));
    record[4] = s_working_brightness;
    record[5] = s_ambient_brightness;
    record[6] = s_watch_face;
    record[7] = Checksum(record);
    BL24C02_Write((uint8_t)(EEPROM_SETTINGS_BASE +
                            s_settings_next_slot * EEPROM_RECORD_SIZE),
                  EEPROM_RECORD_SIZE, record);
    HAL_Delay(6U);
    s_settings_next_slot = (uint8_t)((s_settings_next_slot + 1U) % EEPROM_SETTINGS_SLOTS);
    s_settings_next_sequence++;
}

static void LoadSteps(void)
{
    uint8_t record[EEPROM_RECORD_SIZE];
    uint8_t newest_slot;
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    if(HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) return;
    if(HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) return;
    s_step_year = date.Year;
    s_step_month = date.Month;
    s_step_day = date.Date;

    if(FindNewest8(EEPROM_STEPS_BASE, EEPROM_STEPS_SLOTS,
                   EEPROM_STEPS_MARKER, record, &newest_slot) != 0U) {
        s_steps_next_slot = (uint8_t)((newest_slot + 1U) % EEPROM_STEPS_SLOTS);
        s_steps_next_sequence = (uint8_t)(record[1] + 1U);
        if((record[2] == date.Year) && (record[3] == date.Month) &&
           (record[4] == date.Date)) {
            s_motion.steps_today = (uint32_t)record[5] |
                                   ((uint32_t)record[6] << 8);
        }
        s_last_saved_steps = s_motion.steps_today;
        return;
    }

    BL24C02_Read(EEPROM_LEGACY_STEPS_ADDRESS, EEPROM_RECORD_SIZE, record);
    if((record[0] == EEPROM_STEPS_MARKER) && (record[7] == Checksum(record)) &&
       (record[1] == date.Year) && (record[2] == date.Month) &&
       (record[3] == date.Date)) {
        s_motion.steps_today = (uint32_t)record[4] |
                               ((uint32_t)record[5] << 8) |
                               ((uint32_t)record[6] << 16);
        s_last_saved_steps = s_motion.steps_today;
    }
}

static void SaveSteps(void)
{
    uint8_t record[EEPROM_RECORD_SIZE] = {0};
    uint32_t steps;

    if(s_eeprom_connected == 0U) return;
    steps = s_motion.steps_today;
    if(steps > 65535UL) steps = 65535UL;
    record[0] = EEPROM_STEPS_MARKER;
    record[1] = s_steps_next_sequence;
    record[2] = s_step_year;
    record[3] = s_step_month;
    record[4] = s_step_day;
    record[5] = (uint8_t)steps;
    record[6] = (uint8_t)(steps >> 8);
    record[7] = Checksum(record);
    BL24C02_Write((uint8_t)(EEPROM_STEPS_BASE +
                            s_steps_next_slot * EEPROM_RECORD_SIZE),
                  EEPROM_RECORD_SIZE, record);
    HAL_Delay(6U);
    s_steps_next_slot = (uint8_t)((s_steps_next_slot + 1U) % EEPROM_STEPS_SLOTS);
    s_steps_next_sequence++;
    s_last_saved_steps = s_motion.steps_today;
}

static uint8_t SequenceIsNewer(uint8_t candidate, uint8_t current)
{
    return ((candidate != current) &&
            ((uint8_t)(candidate - current) < 128U)) ? 1U : 0U;
}

static void RecordCompletedDay(void)
{
    uint16_t average_heart = s_heart.bpm;
    float average_temperature = s_environment.temperature;
    float average_humidity = s_environment.humidity;
    if(s_daily_heart_count != 0U)
        average_heart = (uint16_t)(s_daily_heart_sum / s_daily_heart_count);
    if(s_daily_environment_count != 0U) {
        average_temperature = s_daily_temperature_sum / s_daily_environment_count;
        average_humidity = s_daily_humidity_sum / s_daily_environment_count;
    }
    HistoryManager_RecordDay(s_step_year, s_step_month, s_step_day,
                             s_motion.steps_today, average_heart,
                             average_temperature, average_humidity);
    s_daily_heart_sum = 0U;
    s_daily_heart_count = 0U;
    s_daily_temperature_sum = 0.0f;
    s_daily_humidity_sum = 0.0f;
    s_daily_environment_count = 0U;
}

static uint8_t FindNewest8(uint8_t base, uint8_t slots, uint8_t marker,
                           uint8_t *record, uint8_t *newest_slot)
{
    uint8_t candidate[EEPROM_RECORD_SIZE];
    uint8_t slot;
    uint8_t sequence = 0U;
    uint8_t found = 0U;
    for(slot = 0U; slot < slots; slot++) {
        BL24C02_Read((uint8_t)(base + slot * EEPROM_RECORD_SIZE),
                     EEPROM_RECORD_SIZE, candidate);
        if((candidate[0] != marker) || (candidate[7] != Checksum(candidate))) continue;
        if((found == 0U) || SequenceIsNewer(candidate[1], sequence)) {
            memcpy(record, candidate, EEPROM_RECORD_SIZE);
            sequence = candidate[1];
            *newest_slot = slot;
            found = 1U;
        }
    }
    return found;
}

static uint8_t Checksum(const uint8_t *data)
{
    uint8_t checksum = 0x3CU;
    uint8_t index;
    for(index = 0U; index < (EEPROM_RECORD_SIZE - 1U); index++) checksum ^= data[index];
    return checksum;
}

static uint8_t IsDue(uint32_t now, uint32_t deadline)
{
    return ((int32_t)(now - deadline) >= 0) ? 1U : 0U;
}

static uint8_t Weekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t month_table[] = {0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U};
    uint32_t y = year;
    uint32_t weekday;
    if(month < 3U) y--;
    weekday = (y + y / 4U - y / 100U + y / 400U + month_table[month - 1U] + day) % 7U;
    return (weekday == 0U) ? RTC_WEEKDAY_SUNDAY : (uint8_t)weekday;
}
