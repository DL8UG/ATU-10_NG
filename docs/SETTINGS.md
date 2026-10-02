# Settings

> **New in ATU-10 NG: all settings can be changed on the tuner itself.**
> No computer, no editing of the hex file, no flashing – the setup menu on the
> device does it. Editing the hex file still works as before, and a browser
> editor makes that easy too.

There are three ways, for the same 12 settings:

1. the setup menu on the tuner – quick, anywhere
2. the Cell editor in the browser – changes the hex file, with explanations
3. directly in the hex file – with any text editor

## Contents

- [The 12 settings](#the-12-settings)
- [1. Setup menu on the tuner](#1-setup-menu-on-the-tuner)
  - [Opening the menu](#opening-the-menu)
  - [Changing a setting](#changing-a-setting)
  - [The last three pages: SAVE, HEX VALUES, EXIT](#the-last-three-pages-save-hex-values-exit)
  - [Which values are in effect?](#which-values-are-in-effect)
- [2. Cell editor in the browser](#2-cell-editor-in-the-browser)
- [3. Directly in the hex file](#3-directly-in-the-hex-file)
  - [Where the settings are](#where-the-settings-are)
  - [How a line is built](#how-a-line-is-built)
  - [The values are written as decimal digits (BCD)](#the-values-are-written-as-decimal-digits-bcd)
  - [The checksum at the end of each line](#the-checksum-at-the-end-of-each-line)

## The 12 settings

| # | Setting | Meaning | Range | Default | Values in the menu |
|---|---|---|---|---|---|
| 1 | Display off after | minutes without activity, 0 = never | 0..99 | 5 min | 0 (never), 1, 2, 3, 5, 10, 15, 20, 30, 45, 60, 99 |
| 2 | Power off after | minutes without activity, 0 = never | 0..99 | 30 min | 0 (never), 5, 10, 15, 20, 30, 45, 60, 90, 99 |
| 3 | Relay pulse | ms per relay pulse | 2..30 | 7 ms | 3 .. 10, 12, 15, 20, 25, 30 |
| 4 | Min. tune power | in 0.1 W | 1..99 | 1.0 W | 0.1, 0.2, 0.3, 0.5, 0.7, 1.0, 1.5, 2.0, 3.0, 5.0 W |
| 5 | Max. tune power | in W | 1..99 | 15 W | 3, 5, 8, 10, 12, 15, 20 W |
| 6 | Auto tune: SWR change | tune again when the SWR changed by more than (value − 10) / 10; from 90 on practically never | 11..99 | 13 (0.3) | 0.1, 0.2, 0.3, 0.5, 1.0, 1.5, 2.0 |
| 7 | Auto tune | 1 = on, 0 = off | 0..1 | on | off, on |
| 8 | Calibration b (1 W) | detector: b = value / 10 | 0..99 | 4 (b = 0.4) | 0 .. 8, 10, 12, 15, 20 |
| 9 | Calibration a (10 W) | detector: a = 1 + value / 100 | 0..99 | 14 (a = 1.14) | 0 .. 20 in steps of 2, 25, 30, 40, 50 |
| 10 | Power peak hold | in 10 ms | 1..99 | 600 ms | 100, 200, 300, 400, 600, 800, 990 ms |
| 11 | Tune target | stop at SWR 1 + value / 100; 0 = always the full search | 0..99 | 5 (SWR 1.05) | full, 1.02, 1.03, 1.05, 1.08, 1.10, 1.15, 1.20 |
| 12 | Search effort | 1 quick, 2 normal, 3 thorough | 1..3 | 2 (normal) | quick, normal, thorough |

Settings 8 and 9 calibrate the power reading of the detector diodes
(P = a·V² + b·V); the defaults fit the BAT41 diodes of the ATU-10. The tuning
itself compares reflected to forward power and hardly depends on them.

## 1. Setup menu on the tuner

### Opening the menu

Keep the button pressed while switching the tuner on, and keep holding it
through the greeting, until **SETUP** appears. Then let go.

- switched off: press and hold the button – after about 1½ s the greeting
  appears, keep holding for another 3 s
- when connecting the battery: hold the button while connecting it

A short press that only happens to fall on the end of the greeting does not
open the menu – the button has to be held for at least a second.

![SETUP](menu-setup.png)

### Changing a setting

Each page shows the number of the setting, its name and its value in plain
units:

| ![Setting 3, 7 ms](menu-relay.png) | ![after a short press: 8 ms](menu-relay2.png) |
|---|---|
| setting 3 of 12, relay pulse 7 ms | after a short press: 8 ms |

| Button | Action |
|---|---|
| **short press** | next value of the setting shown (after the largest it starts again at the smallest) |
| **long press** (¼ s) | next setting |
| no press for 60 s | the menu ends **without saving** |

The menu offers the values listed in the table above. A value set in the hex
file that is not in the list (for example 13 ms relay pulse) is shown as it
is; the next short press goes to the next value of the list.

### The last three pages: SAVE, HEX VALUES, EXIT

After setting 12 a long press leads to three more pages. On each of them a
**short press** does what the page says; a long press goes on to the next page
(after EXIT back to setting 1).

| ![SAVE](menu-save.png) | ![HEX VALUES](menu-hex.png) | ![EXIT](menu-exit.png) |
|---|---|---|
| **SAVE**: keeps the values as shown and leaves the menu | **HEX VALUES**: back to the values of the hex file | **EXIT**: leaves the menu, changes are dropped |

**SAVE** stores the 12 values in the tuner's data EEPROM. They apply from now
on, also after switching off and after a battery change.

**HEX VALUES** deletes the values that were saved in the menu. From then on
the tuner uses the 12 settings ("Cells") that are written in the firmware hex
file again – for a hex file as published, these are the defaults in the table
above. In detail:

- It deletes only the saved menu values. The hex file and the firmware are
  not changed, the tuned relay setting and the memory of the last 12 tunes
  stay as they are.
- Values changed on the menu pages before are dropped as well – HEX VALUES
  does not save them first.
- It is the way back to a known state: after experimenting in the menu, or if
  you are not sure which values are in effect.
- If you changed the Cells in the hex file (with the editor or by hand) and
  flashed it, you do not need HEX VALUES: changed Cells apply automatically
  (see below).

### Which values are in effect?

The tuner remembers, together with the saved menu values, which Cells the hex
file had at that moment (a checksum of them).

| Situation | In effect |
|---|---|
| nothing saved in the menu yet | the Cells of the hex file |
| saved in the menu | the saved menu values |
| a hex file with **the same** Cells flashed (e.g. the same firmware again) | still the saved menu values |
| a hex file with **other** Cells flashed (edited Cells, or a version with other defaults) | the Cells of the new hex file; the saved menu values are ignored |
| HEX VALUES chosen in the menu | the Cells of the hex file |

The tuner keeps its settings in the data EEPROM. Whether the USB programmer
erases that memory when a new hex file is flashed is not documented; if it
does, the values of the hex file apply after flashing as well.

## 2. Cell editor in the browser

The editor [`tools/cell-editor.html`](../tools/cell-editor.html) is a single
file that runs in any browser, offline – nothing is uploaded anywhere.

![Cell editor with two changed settings](cell-editor.png)

1. Download `cell-editor.html` (on GitHub: open the file, then "Download raw
   file") and open it in the browser.
2. Drop the firmware hex file onto the box at the top, or click the box and
   choose the file.
3. Change the values. Each line shows the range and the default, the column
   *Means* shows what the value means (minutes, watts, SWR ...), and the
   column *Hex* the byte that goes into the file. Changed settings are
   highlighted (green in the picture: settings 1 and 3). An invalid value
   blocks saving.
4. **Save hex file** writes the file (into the downloads folder, under the
   same name). *Default values* sets all 12 to the defaults, *Undo changes*
   goes back to the values of the loaded file.
5. Flash the saved file as usual: copy it onto the tuner's USB drive.

The editor changes only the two lines with the Cells and their checksums;
everything else in the file stays byte for byte the same.

## 3. Directly in the hex file

### Where the settings are

The hex file is a text file in the Intel HEX format. The 12 settings are in
two lines near the end, starting with `:10EEE000` and `:10EEF000`
(address 0xEEE0 in the program memory). In the hex file of version 0.9.0:

```
:10EEE0000534303407341034153413340134043409
:10EEF00014346034053402340034003400340034F7
```

### How a line is built

Taking the first line apart:

```
:  10  EEE0  00  05 34  30 34  07 34  10 34  15 34  13 34  01 34  04 34  09
|  |   |     |   |      |      |      |      |      |      |      |      |
|  |   |     |   Cell1  Cell2  Cell3  Cell4  Cell5  Cell6  Cell7  Cell8  checksum
|  |   |     record type 00 = data
|  |   address EEE0
|  16 data bytes
start of the line
```

- Each setting is a **pair of bytes: the value, then `34`**. Only change the
  value; the `34` must stay (together the two bytes form a "RETLW" instruction
  of the processor).
- The first line holds settings 1 to 8, the second line settings 9 to 12 and
  then four spare pairs `00 34`, which must stay as they are.

### The values are written as decimal digits (BCD)

The value is written with its decimal digits, not converted to hex:
30 minutes are written `30`, 7 ms are `07`, 15 W are `15`. Each of the two
digits must be 0 to 9. A value that is not valid (a letter, like `1A`, or out
of the range in the table) is not used; the tuner takes the default for that
setting instead.

| Setting | Value | written as |
|---|---|---|
| 1 display off after 5 min | 5 | `05` |
| 2 power off after 30 min | 30 | `30` |
| 4 min. tune power 1.0 W (10 x 0.1 W) | 10 | `10` |
| 6 auto tune at SWR change 0.3 | 13 | `13` |
| 10 peak hold 600 ms (60 x 10 ms) | 60 | `60` |
| 11 tune target SWR 1.05 | 5 | `05` |

### The checksum at the end of each line

The last byte of each line is a checksum. After changing a value it has to
be corrected: a line with a wrong checksum is invalid, and the USB programmer
may reject the file or skip the line. The rule: add up all
bytes of the line after the `:` except the checksum; the checksum is what
brings the low byte of that sum to zero (256 minus the low byte).

**Example: relay pulse 7 ms → 12 ms** (setting 3, the third pair in the first
line, `07` becomes `12`):

```
before:  :10EEE000 05 34 30 34 07 34 10 34 15 34 13 34 01 34 04 34 09
after:   :10EEE000 05 34 30 34 12 34 10 34 15 34 13 34 01 34 04 34 FE
```

- sum before: 0x10 + 0xEE + 0xE0 + 0x00 + 0x05 + 0x34 + ... + 0x34 = 0x3F7,
  low byte 0xF7, checksum 0x100 − 0xF7 = **0x09**
- the value grows by 0x12 − 0x07 = 0x0B, the sum becomes 0x402, low byte 0x02,
  checksum 0x100 − 0x02 = **0xFE**

Without the spaces, as it has to be in the file:

```
:10EEE00005343034123410341534133401340434FE
```

Rather than calculating by hand, use the [Cell editor](#2-cell-editor-in-the-browser),
or on the command line `tools/cells.py` from the firmware folder:

```sh
python3 tools/cells.py show ATU-10_NG_0_9_0.hex                    # list the settings
python3 tools/cells.py set ATU-10_NG_0_9_0.hex my.hex 3=12 1=10    # change settings 3 and 1
```

`cells.py set` takes the values in plain decimal (setting=value), writes the
digits and checksums, and leaves the rest of the file unchanged.
