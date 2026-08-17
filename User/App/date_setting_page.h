#ifndef DATE_SETTING_PAGE_H
#define DATE_SETTING_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 创建/销毁日期 roller 页面；必须由 AppUI 生命周期调用。 */
void DateSettingPage_Create(void);
void DateSettingPage_Destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* DATE_SETTING_PAGE_H */
