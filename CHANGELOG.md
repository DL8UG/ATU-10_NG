# Changelog

What changed from version to version, the newest first. Each version is a
tag `vX.Y.Z` in this repository; the hex file is on its release page.

## Contents

- [0.9.6](#096)
- [0.9.5](#095)
- [0.9.4](#094)
- [0.9.3](#093)
- [0.9.2](#092)
- [0.9.1](#091)
- [0.9.0](#090)
- [0.1.0](#010)

## 0.9.6

2026-10-06, [release](https://github.com/DL8UG/ATU-10_NG/releases/tag/v0.9.6), tested on the device

- less current while switched off: the display lines are released instead
  of driven low (with the display module switched on its ground side,
  current flowed through its pull-ups), and the display lines and the lines
  of the external interface get no input buffer while they may be open;
  measured: 15 µA (FW 1.6: 75 µA), so even after LOW BATT the tuner
  survives a few weeks in a drawer without deep discharge. Thanks to
  DL8DTL (Jörg) for his detailed measurements

## 0.9.5

2026-10-04, [release](https://github.com/DL8UG/ATU-10_NG/releases/tag/v0.9.5), tested on the device

- relay pulse 10 ms by default (Cell 3, before 7 ms): the data sheets of
  the Omron G6KU and Panasonic AGN2 relays ask for set and reset pulses
  of 10 ms; a tune step takes about 9 ms longer
- battery: below 3.4 V the battery symbol blinks; below 3.2 V RECHARGE
  shows every 9 seconds (an OVERLOAD message goes first), the tuner still
  tunes; only below 3.0 V LOW BATT and off (before: off below 3.4 V). Each
  level needs three readings in a row (6 seconds), takes the mildest of
  them (a single dip does not switch off) and ends 0.05 V above its
  threshold. When switched on or woken with the battery below 3.0 V,
  LOW BATT shows at once and the relays are not pulsed. LOW BATT is also
  shown when the automatic power off comes at the same moment. Battery
  display and behaviour follow the hints of DL8DTL (Jörg) on the battery's
  cut-off voltages - thanks!
- after the update from an earlier version, settings changed in the setup
  menu are no longer used, because the values in the hex file changed
  (relay pulse): set them again in the menu

## 0.9.4

2026-10-03, [release](https://github.com/DL8UG/ATU-10_NG/releases/tag/v0.9.4), tested on the device

- TUNE without a suitable carrier: after 1 second the display says why,
  in two small lines right of TUNE: WAITING FOR RF (no carrier), POWER TOO
  LOW (below Cell 4) or POWER TOO HIGH (above Cell 5); the power line
  shows the power meanwhile. After 10 seconds the tune gives up with NO
  POWER, TOO LOW or TOO HIGH. A carrier that comes while waiting starts
  the tune after 0.2 seconds, so a short blip does not (before, only TUNE
  was shown for up to 10 seconds, then NO POWER)
- SWR display: the SWR is shown from 0.1 W on, also below the minimum
  tune power of Cell 4 (before, with less than 1.0 W at the default
  setting it showed "-.--" or an old value); auto tune still starts only
  above Cell 4
- LOW BATT: the tuner switches off after three low battery readings
  (below 3.40 V) in a row, 6 seconds; readings between 3.40 and 3.45 V do
  not end the row (before, a single low reading switched it off)

## 0.9.3

2026-10-03, [release](https://github.com/DL8UG/ATU-10_NG/releases/tag/v0.9.3), tested on the device

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

## 0.9.2

2026-10-02, [release](https://github.com/DL8UG/ATU-10_NG/releases/tag/v0.9.2), tested on the device

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

## 0.9.1

2026-10-02, [release](https://github.com/DL8UG/ATU-10_NG/releases/tag/v0.9.1), tested on the device

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

## 0.9.0

2026-10-02, [release](https://github.com/DL8UG/ATU-10_NG/releases/tag/v0.9.0), tested on the device

- display: no left-over pixels of TUNE next to the SWR label
- review fixes: transceiver's bypass pulse also with a dark display, NO POWER
  keeps the tune result, bypass always measured in the final check, display
  restart with ping and back-off, no endless auto tune with large Cell 6
  values, timer comparisons in the main loop

## 0.1.0

2026-10-02, [development version](https://github.com/DL8UG/ATU-10_NG/tree/v0.1.0), no release

- the complete new firmware: search over all relay settings, memory of the
  last 12 good tunes, setup menu, Cells in the hex file and the browser
  editor; simulator and tests on the PC
