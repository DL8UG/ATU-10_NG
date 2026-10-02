# ATU-10 NG firmware 0.1.0

Status: **in development**, not yet tested on the device.

Hex file: `ATU-10_NG_0_1_0.hex` (also in `../ATU-10_NG_0_1_0.zip`).
Flashing, operation and settings: see the [main README](../../README.md).

## Content

First version of the new firmware, written from scratch:

- search over all relay settings: coarse grid on both capacitor sides, local
  search from the best places with valley moves, noise-aware comparisons,
  final check against bypass ([how it works](../../docs/TUNING.md))
- memory of the last 12 good tunes, tried first on every tune
- setup menu on the tuner, Cells in the hex file, browser editor
- relay setting, bypass and memory kept in the data EEPROM
- watchdog, brown-out reset, reset reason on the display
- display drawn from a framebuffer, only changed parts are sent

## Cells of this version

12 Cells at 0xEEE0 (records `:10EEE000`, `:10EEF000`), defaults
`05 30 07 10 15 13 01 04 14 60 05 02`; the last 4 words of the second record
are spare (`00`).

## Build

See [Development](../../docs/DEVELOPMENT.md). Program memory 41 %, RAM 58 %,
hardware stack 9 of 16 levels.
