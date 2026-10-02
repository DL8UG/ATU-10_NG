# Development

## Layout

Each release has its own folder under `Firmware/` with the complete source,
the hex file and the license, and a zip next to it with the hex file and the
license. A released folder is never changed again; the next version starts as
a copy in a new folder.

```
Firmware/ATU-10_NG_0_9_0/
  Makefile           build, checks, tests, simulator, charts
  src/               firmware
  tests/             unit tests on the PC
  tools/             normalize_hex.py, cells.py, sim/ (simulator)
tools/cell-editor.html   Cells editor for users
docs/                    this documentation and its charts
```

## Toolchain

- Microchip XC8 (v4.00), free mode, C99, `-O2`
- the PIC16F1xxxx Device Family Pack (`~/.local/share/microchip/packs/PIC16F1xxxx_DFP/`)
- gcc, python3 and node for the tests and the simulator

```sh
cd Firmware/ATU-10_NG_0_9_0
make            # firmware: ATU-10_NG_0_9_0.hex
make test       # unit tests, Cells tools, editor logic, and the main
                # program on the PC with simulated hardware and time
                # (tests/test_app.c: transmitting, timers, tune, bypass,
                # power off / on, setup menu, external interface, low battery)
make simcompare # simulator: ideal / noise / hard scenario
make simretune  # simulator: QSY after a tune
make docs       # charts, display and setup menu pictures in docs/
                # (rendered with the firmware's display and menu code)
```

`make clean` deletes `build/` only, the hex file stays.

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
2. `make && make test`, simulator runs
3. zip with the hex file and LICENSE, README and LICENSE in the folder, top
   level README (download link, timeline)
