# Development

## Contents

- [Layout](#layout)
- [Toolchain](#toolchain)
- [Tests](#tests)
- [Checks in every build](#checks-in-every-build)
- [Source overview](#source-overview)
- [Simulator](#simulator)
- [License](#license)
- [Release](#release)

## Layout

Each release has its own folder under `Firmware/` with the complete source,
the hex file and the license, and a zip next to it with the hex file and the
license. The source and the hex file of a released folder are never changed
again, nor is its zip; the next version starts as a copy in a new folder.
Only the folder's README may still be updated (e.g. feedback, contents list).

```
LICENSE                  Beerware
README.md                for users
docs/                    this documentation, charts and pictures
tools/cell-editor.html   Cells editor for users (one offline HTML file)
tools/check_doc_links.py links and Contents lists of the documentation
.github/workflows/       on every push: check_doc_links.py, and the host
                         tests of every version folder
Firmware/
  ATU-10_NG_0_9_4.zip    hex file + LICENSE
  ATU-10_NG_0_9_4/
    ATU-10_NG_0_9_4.hex  the firmware
    README.md, LICENSE
    Makefile             build, checks, tests, simulator, charts
    src/                 firmware
    tests/               tests on the PC; tests/host/xc.h stands in for the
                         compiler's register header in test_app.c
    tools/               normalize_hex.py, cells.py, sim/ (simulator)
  ATU-10_NG_0_9_3.zip    the releases before
  ATU-10_NG_0_9_3/       (kept as they are)
  ATU-10_NG_0_9_0.zip
  ATU-10_NG_0_9_0/
  ATU-10_NG_0_1_0/       first development version (kept as it is, README
                         included, so it has no Contents list)
```

## Toolchain

- Microchip XC8 (v4.00), free mode, C99, `-O2` – from microchip.com (on Arch
  Linux also in the AUR); the Makefile finds it under `/opt/microchip/xc8/`
- the PIC16F1xxxx Device Family Pack from
  <https://packs.download.microchip.com>, unpacked to
  `~/.local/share/microchip/packs/PIC16F1xxxx_DFP/<version>/` (XC8 v3 and later
  ship without it)
- gcc, python3 and node for the tests and the simulator

```sh
cd Firmware/ATU-10_NG_0_9_4
make            # firmware: ATU-10_NG_0_9_4.hex
make test       # unit tests, Cells tools, editor logic, and the main
                # program on the PC with simulated hardware and time
                # (tests/test_app.c: transmitting, timers, tune, bypass,
                # power off / on, setup menu, external interface, low battery)
make test-host  # the same without XC8, on the committed hex file
make simcompare # simulator: ideal / noise / hard scenario
make simretune  # simulator: QSY after a tune
make docs       # charts, display and setup menu pictures in docs/
                # (rendered with the firmware's display and menu code)
```

`make clean` deletes `build/` only, the hex file stays.

## Tests

`make test` builds and runs everything on the PC:

| Test | What it checks |
|---|---|
| `test_cells` | BCD decoding of the Cells, invalid values, the Cells checksum |
| `test_meas` | integer power / Pr / Pf / SWR against the floating point formulas |
| `test_antennas` | plausibility of the simulator's antenna models |
| `test_nvm` | EEPROM ring of the relay state, memory slots, damaged data |
| `test_display` | screen layout (also as pictures), display restart and its pauses |
| `test_settings` | menu values in the EEPROM, the rule hex Cells vs menu values |
| `test_tune` | memory slots, bypass always in the final comparison |
| `test_app` | the whole main program with simulated registers and time: 105 minutes of transmitting, timers, tuning, bypass, power off / on, setup menu, external interface, low battery, a missing display; variants 1-25 for start after brown-out / watchdog reset, Cells at minimum / maximum, unmatchable loads, NO POWER, short carriers (CW key), overload, interrupted searches, battery dips, the hints while TUNE waits |
| `test_tools.sh` | `cells.py` and the logic of `cell-editor.html` (run in node): identical files |

Most bugs found in the reviews and on the device have a test that fails with
the code before the fix.

From 0.9.4 on, `make test-host` runs the same tests without XC8, on the
committed hex file. The GitHub workflow "Tests"
([`.github/workflows/tests.yml`](../.github/workflows/tests.yml)) runs them
for every version folder on each push that changes `Firmware/`; the older
folders have no `test-host` and run `make -o <hex> test` instead (the hex
file counts as up to date, so XC8 is not needed).

The documentation has its own check, `python3 tools/check_doc_links.py` at
the top of the repository: every local link and anchor must resolve, and
each Contents list must name all level 2 and 3 headings after it, in order.
GitHub runs it on every push that changes the documentation.

## Checks in every build

- hardware stack depth (16 levels on this PIC, the interrupt included): the
  build fails above 10
- `normalize_hex.py` writes the hex file in the layout the on-board USB
  programmer is known to accept, and refuses one that would lock it out
  (LVP or MCLRE off, code or write protection on)
- `cells.py check --defaults`: the Cells are at 0xEEE0 in their own two
  records and hold the defaults

## Source overview

| File | Content |
|---|---|
| `app.c` | main loop, tuning and bypass, display content, power off |
| `tune.c` | the search (free of hardware access, also built in the simulator) |
| `meas_math.c` | power, Pr / Pf, SWR, noise estimate (integer only, shared with the simulator) |
| `meas.c` | ADC with three reference ranges |
| `relays.c` | relay pulses |
| `nvm.c` | data EEPROM: relay state ring, memory of good tunes |
| `cells.c`, `settings.c`, `setup.c` | Cells in the hex file, settings block, setup menu |
| `display.c`, `oled.c`, `i2c_soft.c` | framebuffer, SSD1306, I2C |
| `buttons.c`, `timer.c` | button / external interface events, 1 ms tick |
| `board.c`, `config.c` | pins, modules, configuration words |

Data EEPROM: 0x20..0x2F settings block, 0x30..0x6B memory of good tunes (12
slots, each with its own CRC: a tune writes only its slot), 0x70..0xEF relay
state ring (16 slots); the rest is not used.

## Simulator

`tools/sim/sim.c` with `model.c` (network, bridge, detectors, ADC) and
`antennas.c` (antenna, feed line and transformer models). `glue_new.c` runs the
firmware's `tune.c` and `meas_math.c`. Options: see the comment at the top of
`sim.c`. `compare.py` summarizes runs per antenna type.

For development only, `make build/sim_ref REF_DIR=...` builds another tuning
algorithm against the same model for comparison.

## License

Beerware ([LICENSE](../LICENSE), SPDX `Beerware`). Parts that follow N7DDC's
ATU-10 firmware (public domain) say so at the top of their file: pin
assignment (`board.h`), relay pulse sequence (`relays.c`), display
initialization (`oled.c`), font (`font5x8.h`).

## Release

1. new folder `Firmware/ATU-10_NG_x_y_z` as a copy of the last one, version in
   `src/version.h` and `Makefile`
2. `make && make test`, simulator runs, `python3 tools/check_doc_links.py`
3. zip with the hex file and LICENSE, README and LICENSE in the folder, top
   level README (download link, timeline)
4. commit, push, then a GitHub release with the tag `vX.Y.Z` and the hex
   file, the zip and LICENSE attached, e.g.

   ```sh
   gh release create v0.9.4 Firmware/ATU-10_NG_0_9_4/ATU-10_NG_0_9_4.hex \
      Firmware/ATU-10_NG_0_9_4.zip Firmware/ATU-10_NG_0_9_4/LICENSE \
      --title "ATU-10 NG 0.9.4" --notes-file notes.md   # --prerelease for a beta
   ```
