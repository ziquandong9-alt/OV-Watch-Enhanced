# Display performance notes

## Current observation

On the current watch hardware, the menu's observed minimum LVGL FPS improved from approximately 14 to approximately 40 during development. A reference OV-Watch build was observed at approximately 21 FPS in a similar sliding scenario.

These are engineering observations, not yet a controlled cross-project benchmark. They should only be quoted together with this qualification.

## Why partial refresh can exceed full-screen theoretical FPS

SPI bandwidth limits the number of pixels transferred per second, not the number of times LVGL may finish a small dirty-region frame. A moving second hand can invalidate a narrow region and report around 60 FPS even when a 240×280 full-screen transfer cannot reach 60 FPS.

Therefore, FPS should be evaluated with:

- invalidated pixel area;
- LCD flush pixel count and duration;
- maximum frame time;
- minimum, median and percentile FPS;
- identical compiler optimization and SPI configuration.

## Reproducible benchmark protocol

Before publishing a strict comparison:

1. Use the same physical watch, SPI clock, LVGL draw buffers and compiler optimization.
2. Start from the same menu scroll position.
3. Perform a recorded sequence of 20 equivalent upward/downward swipes.
4. Capture the display with a high-frame-rate camera.
5. Record minimum, median and P1 FPS rather than selecting one best run.
6. Save the tested firmware commit and HEX hash.
7. Publish both videos without cuts and state any functional differences between builds.

## Relevant implementation

- `User/App/menu_page.c`: custom item draw callback and transparent backgrounds.
- `User/WatchFace/watch_face.c`: interpolated second hand and local invalidation.
- `ThirdParty/LVGL/GUI/lv_conf.h`: built-in performance monitor.
- `MDK-ARM/My_OV_Watch.uvprojx`: compiler optimization configuration.
