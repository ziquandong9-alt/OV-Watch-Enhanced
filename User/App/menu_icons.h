#ifndef MENU_ICONS_H
#define MENU_ICONS_H

#include "lvgl.h"

LV_FONT_DECLARE(ui_font_iconfont30);
LV_FONT_DECLARE(ui_font_iconfont34);

/*
 * 图标字体把图形放在 Unicode 私用区，下面写的是这些码点的 UTF-8 字节。
 * 阅读提示：若图标显示成方框，先检查 label 使用的字体是否真的包含对应码点，
 * 不要把问题误判为字符串编码错误。
 */
#define MENU_ICON_CALENDAR       "\xEE\x98\x81" /* U+E601 */
#define MENU_ICON_TIMER          "\xEE\x98\xB3" /* U+E633 */
#define MENU_ICON_HEART_RATE     "\xEE\x9D\xA2" /* U+E762 */
#define MENU_ICON_BLOOD_OXYGEN   "\xEE\x99\x92" /* U+E652 */
#define MENU_ICON_ENVIRONMENT    "\xEE\x9C\x86" /* U+E706 */
#define MENU_ICON_COMPASS        "\xEE\x9E\x88" /* U+E788 */
#define MENU_ICON_SETTINGS       "\xEE\x98\x80" /* U+E600 */
#define MENU_ICON_ABOUT          "\xEE\x98\x8B" /* U+E60B */
#define MENU_ICON_CALCULATOR     "="

#endif /* MENU_ICONS_H */
