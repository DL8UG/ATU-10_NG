# ATU-10 NG firmware 0.9.4

Status: **in development, not yet tested on the device**.

Hex file: `ATU-10_NG_0_9_4.hex`.
Flashing, operation and settings: see the [main README](../../README.md).

## Contents

- [Features](#features)
- [Changes since 0.9.2](#changes-since-092)
- [Changes since 0.9.1](#changes-since-091)
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

## Changes since 0.9.2

- tuning with short carriers: a tune that goes on with an interrupted
  search first measures the present setting again; when it measures
  clearly different than before (other band, other antenna) or bypass was
  switched on meanwhile, a new search starts (before, the search went on
  with the measurements of the old band, or took bypass as its best
  setting so far)
- tuning with short carriers: switching off ends the interrupted search
  (before, after the automatic power off or LOW BATT the next tune could
  still go on with it, as the minute only counts while switched on)
- tuning with short carriers: a tune that measured nothing new, e.g. with
  carriers too short for one measurement, ends the chain (before, every
  such carrier started another tune, with the key line to the transceiver
  low for up to 10 seconds each time)

## Changes since 0.9.1

- auto tune: after a tune that changed nothing (STOP, NO POWER, OVERLOAD)
  the same tune is not started again at the same SWR (before, a STOP was
  followed by a new tune 3 seconds later while you kept transmitting)
- tuning with short carriers, e.g. the CW key pressed for a second with
  pauses of more than about 5 seconds: the next tune within a minute goes
  on with the interrupted search instead of starting again, and auto tune
  starts it with the next carrier while the SWR is above 1.2 (before, the
  tuner kept the half-finished result)
- too much power: when the detector is above its measuring range (about
  14 W forward power at an empty battery, 22 W at a full one) at 64 relay
  settings in a row, the tune stops with OVERLOAD and the relays stay as
  they were (before the search went on through all its relay steps under
  that power)
- display: during a tune the SWR found so far is shown also when it is the
  same as before the tune
- display: SWR "-.--" until the first measurement (before "0.00")
- display: only changed parts are sent (before, e.g. the battery symbol went
  over I2C every 3 seconds)
- memory of good tunes: after about 128 tunes on one band its order got
  mixed up, and a band used a moment ago could be pushed out instead of the
  oldest one
- switched off: the ADC and its voltage reference are off (less current
  from the battery)

## Changes since 0.9.0

- greeting on two pages: "ATU-10 / HARDWARE BY N7DDC", then the version
  "NG 0.9.1 / FIRMWARE BY DL8UG", 2 seconds each (before: one page, 3 seconds)
- power off: the display is cleared and POWER OFF (or LOW BATT) stands alone
  in its middle
- NO MATCH also when the network cannot improve the antenna and the SWR in
  bypass is above 1.2 (before: only at SWR 9.99); at or below 1.2 the
  antenna is simply left as it is
- tune target 0 (setting 11, "always the full search") now also applies
  when a remembered setting is about as good as before
- auto tune: no endless TUNE / NO POWER cycle when the transmit power is
  just below the minimum tune power (e.g. 0.97 W at a 1.0 W minimum)
- the transceiver's bypass pulse during a tune stops the tune and switches
  bypass on (before it was lost)
- when the step budget ends the search during the coarse grid (mostly at
  search effort 1), the best grid point takes part in the final comparison
- no EEPROM write after a tune that changed nothing (NO POWER, STOP)

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

See [Development](../../docs/DEVELOPMENT.md). Program memory 43 %, RAM 58 %,
hardware stack 9 of 16 levels.
