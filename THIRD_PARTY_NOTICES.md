# Third-party notices

This repository contains or depends on work from multiple sources. Copyright and license notices inside third-party files must remain intact.

## OV-Watch

- Source: https://github.com/No-Chicken/OV-Watch
- License: GNU General Public License v3.0

This repository is an unofficial modified version. See `ATTRIBUTION.md`.

## STM32 CMSIS and HAL

License files are retained under:

- `Drivers/CMSIS/LICENSE.txt`
- `Drivers/CMSIS/Device/ST/STM32F4xx/LICENSE.txt`
- `Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`

## LVGL

LVGL source is stored under `ThirdParty/LVGL`. Preserve all upstream copyright and license headers when updating or redistributing it.

## Fredoka

The jelly watch face digit subset in `User/App/Fonts/ui_font_Fredoka135.c`
is generated from Fredoka Bold:

- Copyright 2016 The Fredoka Project Authors
- Source: https://github.com/hafontia-zz/Fredoka-One
- Source revision: `35c584ff23450c9bcdf8819706e12fcdeefe1712`
- License: SIL Open Font License 1.1
- Local license copy: `User/App/Fonts/Fredoka-OFL.txt`

The generated subset contains only `0123456789:` and may be embedded and
redistributed with the firmware under the terms of the OFL.

## Material Icons

The application icon subsets in `User/App/Fonts/ui_font_material_icons*.c` are
generated from Google's official Material Icons Round font:

- Project: Material Design Icons by Google
- Source: https://github.com/google/material-design-icons
- Source revision: `e083cc60a0828fdd3b404cea0cb8a5b900e9c23e`
- Source font: `font/MaterialIconsRound-Regular.otf`
- License: Apache License 2.0
- Local license copy: `User/App/Fonts/MaterialIcons-LICENSE.txt`

Only the eleven glyphs required by the watch UI are included. The previous
unverified `iconfont.ttf` glyph data has been completely replaced and is not
redistributed by this repository.

## font8x8 Basic Latin

The raw-LCD diagnostic text renderer uses the public-domain 8x8 Basic Latin
bitmap font:

- Project: font8x8 by Daniel Hepper
- Source: https://github.com/dhepper/font8x8
- Source revision: `8e279d2d864e79128e96188a6b9526cfa3fbfef9`
- License: Public Domain
- Local adapted copy: `BSP/LCD/font8x8_basic.h`

The old raw-LCD Chinese glyph tables were removed. Application text is rendered
through LVGL; the raw-LCD renderer remains only for diagnostic ASCII output.

## Removed unused font assets

The disabled LVGL `lv_font_simsun_16_cjk.c` source, unused `korean.ttf`, and
unused FreeType sample `arial.ttf` were removed from this distribution. None
was required by the configured firmware.

Do not add Apple promotional images, Apple logos or other reference artwork to this repository. The jelly-style watch face should be documented as an independently implemented design inspired by a general visual style, not as an official Apple watch face.
