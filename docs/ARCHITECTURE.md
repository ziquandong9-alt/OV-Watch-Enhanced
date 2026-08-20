# Software architecture

## Main-loop ownership

The firmware uses a cooperative main loop. Hardware managers update cached state and generate one-shot events; the UI reads the cache and decides how those events are presented.

```text
DeviceManager_Process()
        │
        ├─ scheduled sensor samples
        ├─ step/date/alert processing
        ├─ EEPROM persistence
        └─ one-shot events
                 │
                 ▼
AppUI_Process() ─────► page Create/Destroy
        │
        ├─ normal display
        ├─ ambient display (AOD)
        ├─ STOP mode
        └─ wake and deferred overlays
```

## Deferred page transitions

LVGL event callbacks call `AppUI_RequestPage()` rather than deleting their current page immediately. `AppUI_Process()` consumes the request after event processing and performs a strict destroy-then-create transition. This prevents callbacks and timers from retaining pointers to objects deleted inside their own event stack.

Each page owns its timer lifecycle. `Create` allocates objects and timers; `Destroy` stops timers, releases sensor high-rate modes and clears static object pointers.

## Display rendering

The classic watch face separates the dynamic second hand from the static meter. The second-hand endpoint is interpolated at fractional-degree precision. Only bounding areas covering the old and new hand positions are invalidated.

The menu uses one clickable panel per item. Icons and labels are emitted from the panel's draw callback, reducing nested LVGL objects and style traversal. Transparent list/item backgrounds avoid filling the same black background multiple times.

## Power states

`AppUI` coordinates normal display, AOD and STOP. `PowerManager_StopOnce()` suspends SysTick, enters STOP, identifies the wake source, restores the system clock and compensates elapsed software time where required.

Wake input is deliberately consumed as wake input first; it is not immediately reinterpreted as an unrelated page action. This is especially important for a key that remains physically pressed after waking.

## EEPROM records

Settings, time and steps occupy separate EEPROM regions. Circular slots distribute writes across addresses. A marker and checksum reject incomplete or corrupt entries; an 8-bit sequence number selects the newest valid entry.

Sequence comparison uses modular half-range ordering, so `0` is correctly newer than `255` while ambiguous distances of 128 or more are rejected as older. Versioned settings records retain compatibility with earlier fixed formats.

The activity goal is encoded in spare settings flag bits in 500-step units. Goal completion produces a one-shot event. AOD and STOP do not consume it, so the celebration appears after a real wake rather than being rendered onto a sleeping display.
