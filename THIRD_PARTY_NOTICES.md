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

## Icon glyph data — publication review required

Generated icon files under `User/App/Fonts` reference `iconfont.ttf`. No
standalone redistribution license for that source font was found in this local
project during the August 2026 publication audit. Confirm its source license or
replace the icon glyphs before changing this repository from private to public.

Do not add Apple promotional images, Apple logos or other reference artwork to this repository. The jelly-style watch face should be documented as an independently implemented design inspired by a general visual style, not as an official Apple watch face.
