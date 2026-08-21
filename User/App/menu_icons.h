#ifndef MENU_ICONS_H
#define MENU_ICONS_H

#include "lvgl.h"

LV_FONT_DECLARE(ui_font_material_icons30);
LV_FONT_DECLARE(ui_font_material_icons34);

/*
 * 图标字体把图形放在 Unicode 私用区，下面写的是这些码点的 UTF-8 字节。
 * 阅读提示：若图标显示成方框，先检查 label 使用的字体是否真的包含对应码点，
 * 不要把问题误判为字符串编码错误。
 */
#define MENU_ICON_CALENDAR       "\xEE\xA4\xB5" /* U+E935 calendar_today */
#define MENU_ICON_TIMER          "\xEE\x90\xA5" /* U+E425 timer */
#define MENU_ICON_HEART_RATE     "\xEE\xA1\xBD" /* U+E87D favorite */
#define MENU_ICON_ENVIRONMENT    "\xEE\xA8\xB5" /* U+EA35 eco */
#define MENU_ICON_COMPASS        "\xEE\xA1\xBA" /* U+E87A explore */
#define MENU_ICON_SETTINGS       "\xEE\xA2\xB8" /* U+E8B8 settings */
#define MENU_ICON_ABOUT          "\xEE\xA2\x8E" /* U+E88E info */
#define MENU_ICON_CALCULATOR     "="

#endif /* MENU_ICONS_H */
