# Attribution and modification notice

## Upstream

- Project: OV-Watch
- Author/maintainer: No-Chicken / 不吃油炸鸡
- Canonical repository: https://github.com/No-Chicken/OV-Watch
- License: GNU General Public License v3.0

This repository is an unofficial modified work derived from OV-Watch. It is not an official release of the upstream project and is not endorsed by the upstream author.

The upstream authors and third-party contributors retain copyright in their respective original contributions. No upstream copyright or license notice is intentionally removed.

## Modifications

Substantial modifications were made by `dong8` / GitHub user `ziquandong9-alt` in August 2026. The changes include, but are not limited to:

- reorganizing the application into App, Device, Pages, Power and WatchFace layers;
- cooperative device scheduling and UI state management;
- LVGL partial invalidation and menu rendering performance work;
- three watch faces with snap scrolling and persistent selection;
- AOD/STOP/wake state handling and power-latch shutdown support;
- EEPROM wear leveling, checksums, sequence wraparound and format migration;
- notifications, history, control center and daily activity goal workflow;
- extensive Chinese comments and learning-oriented documentation.

All modifications in this repository are distributed under GNU GPL v3.0 as part of the modified work. See `LICENSE` for the complete terms.
