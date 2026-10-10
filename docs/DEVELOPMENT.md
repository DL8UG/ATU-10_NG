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

The source of the current version is at the top of the repository. Each
version is a tag `vX.Y.Z`; its hex file, a zip (hex file + license) and the
license are on the GitHub release of that tag. The hex file is built, not
committed.

```
LICENSE                  Beerware
README.md                for users
CHANGELOG.md             changes from version to version
Makefile                 build, checks, tests, simulator, charts
src/                     firmware
tests/                   tests on the PC; tests/host/xc.h stands in for the
                         compiler's register header in test_app.c
tools/                   normalize_hex.py, cells.py, sim/ (simulator),
                         cell-editor.html (Cells editor for users, one
                         offline HTML file), check_doc_links.py (links and
                         Contents lists of the documentation), sim-fw16/
                         (FW 1.6 search in the simulator)
docs/                    this documentation, charts and pictures
.github/workflows/       on every push: build and tests, documentation check
.github/ISSUE_TEMPLATE/  form for problem reports
build/                   (not committed) hex file, map, test programs
dist/                    (not committed) files for a release: make dist
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
make            # firmware: build/ATU-10_NG_x_y_z.hex (version from src/version.h)
make test       # unit tests, Cells tools, editor logic, and the main
                # program on the PC with simulated hardware and time
                # (tests/test_app.c: transmitting, timers, tune, bypass,
                # power off / on, setup menu, external interface, low battery)
make dist       # dist/: hex file, zip (hex file + LICENSE), LICENSE
make simcompare # simulator: ideal / noise / hard scenario
make simretune  # simulator: QSY after a tune
make docs       # charts, display and setup menu pictures in docs/
                # (rendered with the firmware's display and menu code)
```

`make clean` deletes `build/` and `dist/`.

## Tests

`make test` builds and runs everything on the PC:

| Test | What it checks |
|---|---|
| `test_cells` | BCD decoding of the Cells, invalid values, the Cells checksum |
| `test_meas` | integer power / Pr / Pf / SWR against the floating point formulas |
| `test_antennas` | plausibility of the simulator's antenna models |
| `test_nvm` | EEPROM ring of the relay state, memory slots, damaged data |
| `test_display` | screen layout (also as pictures, the relay view too), relay cells and the small battery, nothing in the columns 126 and 127 the display does not show, display restart and its pauses, the display lines when switched off |
| `test_settings` | menu values in the EEPROM, the rule hex Cells vs menu values, a block saved by 1.0.0 still applies, setting 13 in its own block |
| `test_tune` | memory slots, bypass always in the final comparison |
| `test_app` | the whole main program with simulated registers and time: 105 minutes of transmitting, timers, tuning, bypass, power off / on (pins without current while sleeping), setup menu, external interface, low battery, a missing display; variants 1-31 for start after brown-out / watchdog reset, Cells at minimum / maximum, unmatchable loads, NO POWER, short carriers (CW key), overload, interrupted searches, battery dips and levels (blinking symbol, RECHARGE, a dip at the end of a row, an overload while RECHARGE shows, LOW BATT in the same moment as the power off, an empty battery at the start), the hints while TUNE waits, the relay view (the cells follow the best setting while tuning and the relays after it, bypass) |
| `test_tools.sh` | `cells.py` and the logic of `cell-editor.html` (run in node): identical files |

Most bugs found in the reviews and on the device have a test that fails with
the code before the fix.

The GitHub workflow "Tests"
([`.github/workflows/tests.yml`](../.github/workflows/tests.yml)) runs
`make test` on each push that changes the source, the tests, the tools or
the Makefile. It downloads XC8 and the Device Family Pack from Microchip
(checked by their SHA-256, then cached) and keeps the hex file as an
artifact of the run; it is the same, byte for byte, as a local build.

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

`make simcompare REF_DIR=<folder>` runs another search algorithm (`tune.c`,
`swr.c` in that folder) against the same model and shows both side by side,
e.g. `REF_DIR=tools/sim-fw16` for the search of FW 1.6.

## License

Beerware ([LICENSE](../LICENSE), SPDX `Beerware`). Parts that follow N7DDC's
ATU-10 firmware (public domain) say so at the top of their file: pin
assignment (`board.h`), relay pulse sequence (`relays.c`), display
initialization (`oled.c`), font (`font5x8.h`).

## Release

1. version in `src/version.h`, the changes in `CHANGELOG.md`
2. `make test`, simulator runs, `python3 tools/check_doc_links.py`
3. `make docs` (the greeting picture shows the new version), top level README
   (status, download, timeline)
4. commit, tag `vX.Y.Z`, push both
5. `make dist`, then a GitHub release of the tag with the files of `dist/`,
   e.g.

   ```sh
   gh release create v1.0.1 dist/* \
      --title "ATU-10 NG 1.0.1" --notes-file notes.md   # --prerelease for a beta
   ```
