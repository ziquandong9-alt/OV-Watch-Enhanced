#include "about_page.h"
#include "lvgl.h"
#include "lcd_init.h"

void AboutPage_Create(void)
{
    /* 关于页全是静态 label，不需要 timer 或模块级对象指针。 */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);

    /* 根屏幕使用不透明黑色，保证圆角屏边缘没有旧页面残影。 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x05070AU), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label_title = lv_label_create(scr);
    lv_label_set_text(label_title, "About");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *label_author = lv_label_create(scr);
    lv_label_set_text(label_author, "author: dzq");
    lv_obj_set_style_text_font(label_author, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_author, lv_color_hex(0xC8C8C8U), LV_PART_MAIN);
    lv_obj_align(label_author, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *label_ver = lv_label_create(scr);
    lv_label_set_text(label_ver, "version: v1.1.1");
    lv_obj_set_style_text_font(label_ver, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_ver, lv_color_hex(0xC8C8C8U), LV_PART_MAIN);
    lv_obj_align(label_ver, LV_ALIGN_TOP_MID, 0, 115);

    /* 底部提示：按实体按键返回菜单 */
    lv_obj_t *label_hint = lv_label_create(scr);
    lv_label_set_text(label_hint, "Press KEY1 back");
    lv_obj_set_style_text_font(label_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_hint, lv_color_hex(0x606060U), LV_PART_MAIN);
    lv_obj_align(label_hint, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void AboutPage_Destroy(void)
{
    /* 所有 label 都是根屏幕子对象，clean 会递归释放。 */
    lv_obj_clean(lv_scr_act());
}
