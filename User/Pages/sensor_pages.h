#ifndef SENSOR_PAGES_H
#define SENSOR_PAGES_H

/* 各传感器页面均遵循 Create 开启采样、Destroy 恢复低功耗。 */
void MotionPage_Create(void);
void MotionPage_Destroy(void);
void MotionGoalPage_Create(void);
void MotionGoalPage_Destroy(void);
void HeartPage_Create(void);
void HeartPage_Destroy(void);
void EnvironmentPage_Create(void);
void EnvironmentPage_Destroy(void);
void CompassPage_Create(void);
void CompassPage_Destroy(void);

#endif
