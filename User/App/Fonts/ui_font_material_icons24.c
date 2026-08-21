/*******************************************************************************
 * Material Icons Round by Google, licensed under Apache License 2.0.
 * See User/App/Fonts/MaterialIcons-LICENSE.txt.
 * Source: https://github.com/google/material-design-icons
 * Source revision: e083cc60a0828fdd3b404cea0cb8a5b900e9c23e
 * Glyphs: directions_walk (U+E536)
 * Size: 24 px
 * Bpp: 4
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef UI_FONT_MATERIAL_ICONS24
#define UI_FONT_MATERIAL_ICONS24 1
#endif

#if UI_FONT_MATERIAL_ICONS24

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+E536 "" */
    0x0, 0x0, 0x0, 0x7a, 0x40, 0x0, 0x0, 0x0,
    0x0, 0x8f, 0xff, 0x10, 0x0, 0x0, 0x0, 0xa,
    0xff, 0xf3, 0x0, 0x0, 0x0, 0x0, 0x4f, 0xfc,
    0x0, 0x0, 0x0, 0x0, 0x2, 0x23, 0x0, 0x0,
    0x0, 0x0, 0x6d, 0xff, 0x50, 0x0, 0x0, 0x8,
    0xff, 0xff, 0xff, 0x20, 0x0, 0xb, 0xff, 0xef,
    0xff, 0xfd, 0x0, 0x0, 0xff, 0x37, 0xff, 0xff,
    0xfb, 0x0, 0xf, 0xf0, 0xaf, 0xff, 0xaf, 0xfe,
    0x81, 0xff, 0xd, 0xff, 0xf0, 0x3d, 0xff, 0xd7,
    0x70, 0xff, 0xfd, 0x0, 0x3, 0x86, 0x0, 0x3f,
    0xff, 0xd1, 0x0, 0x0, 0x0, 0x6, 0xff, 0xff,
    0xd1, 0x0, 0x0, 0x0, 0x9f, 0x98, 0xff, 0x90,
    0x0, 0x0, 0xc, 0xf5, 0x8, 0xfc, 0x0, 0x0,
    0x0, 0xff, 0x20, 0x3f, 0xc0, 0x0, 0x0, 0x3f,
    0xf0, 0x3, 0xfc, 0x0, 0x0, 0x6, 0xfb, 0x0,
    0x3f, 0xc0, 0x0, 0x0, 0x9f, 0x80, 0x3, 0xfc,
    0x0, 0x0, 0xb, 0xf4, 0x0, 0x3f, 0xc0, 0x0,
    0x0, 0x5a, 0x0, 0x0, 0xa5, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 384, .box_w = 13, .box_h = 22, .ofs_x = 6, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 58678, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_material_icons24 = {
#else
lv_font_t ui_font_material_icons24 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 22,          /*The maximum line height required by the font*/
    .base_line = -1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_MATERIAL_ICONS24*/
