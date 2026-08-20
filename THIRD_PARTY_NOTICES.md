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

## Font and icon glyph data — publication review required

Generated files under `User/App/Fonts` include metadata referencing `方正粗圆简体.TTF` and `iconfont.ttf`. No standalone redistribution license for those source fonts was found in this local project during the August 2026 publication audit.

The glyph data was inherited from or derived from assets used by the upstream project, but that fact alone is not independent proof of the underlying font license. Before changing the GitHub repository from private to public, the maintainer should do one of the following:

1. obtain and document permission covering embedded glyph redistribution; or
2. replace the glyph data with a font carrying a clearly compatible open-font license and include that license.

Do not add Apple promotional images, Apple logos or other reference artwork to this repository. The jelly-style watch face should be documented as an independently implemented design inspired by a general visual style, not as an official Apple watch face.
