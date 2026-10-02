# ATU-10 NG firmware 0.9.1

Status: **in development, not yet tested on the device**.

Hex file: `ATU-10_NG_0_9_1.hex`.
Flashing, operation and settings: see the [main README](../../README.md).

## Contents

- [Features](#features)
- [Changes since 0.9.0](#changes-since-090)
- [Changes since 0.1.0 (development version)](#changes-since-010-development-version)
- [Cells of this version](#cells-of-this-version)
- [Feedback](#feedback)
- [License](#license)
- [Build](#build)

## Features

The new firmware, written from scratch:

- search over all relay settings: coarse grid on both capacitor sides, local
  search from the best places with valley moves, noise-aware comparisons,
  final check against bypass ([how it works](../../docs/TUNING.md))
- memory of the last 12 good tunes, tried first on every tune
- setup menu on the tuner, Cells in the hex file, browser editor
- relay setting, bypass and memory kept in the data EEPROM (wear spread)
- watchdog, brown-out reset, reset reason on the display
- display drawn from a framebuffer, only changed parts are sent; a display
  that does not answer is restarted with growing pauses

## Changes since 0.9.0

- (in progress)

## Changes since 0.1.0 (development version)

- display: no left-over pixels of TUNE next to the SWR label
- review fixes: transceiver's bypass pulse also with a dark display, NO POWER
  keeps the tune result, bypass always measured in the final check, display
  restart with ping and back-off, no endless auto tune with large Cell 6
  values, timer comparisons in the main loop

## Cells of this version

12 Cells at 0xEEE0 (records `:10EEE000`, `:10EEF000`), defaults
`05 30 07 10 15 13 01 04 14 60 05 02`; the last 4 words of the second record
are spare (`00`).

## Feedback

Problems, questions and reports from use on the air are very welcome. Please
report them with a detailed description

- as an [issue here on GitHub](https://github.com/DL8UG/ATU-10_NG/issues), or
- in the groups.io thread [ATU-10 NG firmware](https://groups.io/g/ATU100/topic/atu_10_ng_firmware/121543821).

Please include: the firmware version (shown in the greeting), the antenna and
feed line, band and frequency, transceiver and power, what the display showed
(SWR, messages), what you expected instead, and how to make it happen again.

## License

Beerware, see [LICENSE](LICENSE) (the same as in the top folder).

## Build

See [Development](../../docs/DEVELOPMENT.md). Program memory 41 %, RAM 58 %,
hardware stack 9 of 16 levels.
