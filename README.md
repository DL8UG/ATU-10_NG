# ATU-10 NG – new firmware for the ATU-10 QRP antenna tuner

A new firmware for the ATU-10 automatic antenna tuner (hardware design by
N7DDC), written from scratch. Its aim is the best possible match and a calm,
stable tuner – not the fastest tune.

![Display](docs/display-main.png)

**Status: 0.9.0, tested on the device.** See the [timeline](#development-status) below.

## What it does

- **Finds the best match, not the first one.** The tuner looks at the whole
  range of relay settings first and then refines the most promising ones. In
  the simulator it reaches the best possible SWR (within 0.05) in practically
  all cases that can be matched at all (over 99.9 %), with random wires, EFHWs, dipoles,
  doublets and more. [How tuning works](docs/TUNING.md)
- **Remembers the last 12 good tunes.** Back on a band you used before, the
  tuner tries the remembered settings first: a band change then takes about
  2 seconds instead of 4 to 5.
- **Bypass** with a short press, and back to the tuned setting.
- **Settings in three ways:** in a menu on the tuner, with an editor in your
  browser, or directly in the hex file.
- **Keeps its setting** after switching off, a reset or a battery change.
- **Can always be updated** over USB, to any other firmware too.

## Download

Version 0.9.0 (not yet published): [`Firmware/ATU-10_NG_0_9_0.zip`](Firmware/ATU-10_NG_0_9_0.zip)
contains the hex file `ATU-10_NG_0_9_0.hex`.

## Flashing

1. Connect the tuner to the computer with a USB cable.
2. A USB drive appears. Copy the `.hex` file onto it.
3. Wait until the tuner restarts and shows the greeting
   "ATU-10 / FW NG 0.9.0 / DESIGNED BY DL8UG".

Any other firmware hex file for the ATU-10 can be flashed the same way at any
time – this firmware never blocks the way back.

## Operation

| Button | Action |
|---|---|
| short press | bypass on / off (back to the tuned setting) |
| long press (¼ s) | tune – send a carrier of 1 to 15 W |
| very long press (2½ s) | switch off; to switch on, hold the button for about 1½ s |
| any press while the display is dark | display on |

Tuning needs a steady carrier (CW, FM or AM, 1 to 15 W; 2 to 5 W gives the
most exact readings). A short press during tuning stops it; the tuner keeps
the best setting found so far.

The display shows the power (with a peak hold) and the SWR, or BYP while in
bypass. Messages: TUNE, NO POWER (no carrier), NO MATCH (nothing better than
bypass), STOP, OVERLOAD (too much power), LOW BATT.

The tuner tunes again by itself (auto tune) when the SWR has changed by more
than 0.3 and is above 1.2 – this can be switched off.

External interface: a short pulse on the start line switches the bypass on, a
long one starts a tune; the key line is held low while tuning.

## Settings

| # | Setting | Default |
|---|---|---|
| 1 | Display off after (minutes, 0 = never) | 5 |
| 2 | Power off after (minutes, 0 = never) | 30 |
| 3 | Relay pulse (ms) | 7 |
| 4 | Minimum power for tuning (in 0.1 W) | 1.0 W |
| 5 | Maximum power for tuning (W) | 15 W |
| 6 | Auto tune when the SWR changed by more than (from 9.0 on: practically never) | 0.3 |
| 7 | Auto tune on / off | on |
| 8 | Detector calibration b (1 W) | 4 |
| 9 | Detector calibration a (10 W) | 14 |
| 10 | Power peak hold | 600 ms |
| 11 | Tune target: stop at this SWR (0 = always the full search) | 1.05 |
| 12 | Search effort: 1 quick, 2 normal, 3 thorough | 2 |

**Menu on the tuner:** keep the button pressed when switching on, until SETUP
appears. A short press changes the value, a long press goes to the next
setting. At the end: SAVE, HEX VALUES (back to the values of the hex file) or
EXIT. Without a press for a minute the menu ends without saving.

**Editor in the browser:** open [`tools/cell-editor.html`](tools/cell-editor.html)
(download it and open it; it works offline), load the hex file, change the
values and save. Then flash the file.

**In the hex file:** the settings are 12 BCD coded bytes ("Cells") at address
0xEEE0, in the lines starting with `:10EEE000` (settings 1–8) and `:10EEF000`
(9–12). Each setting is a pair of bytes: the value and `34`. Example: `07 34`
= 7 ms relay pulse. After changing a line its checksum (last byte) must be
corrected; the editor does that for you.

Values saved in the menu take precedence over the hex file – until you flash
a hex file with other settings; then those apply.

## Questions

**The tuner does not find a good match.** Check the power (1 to 15 W) and that
the carrier is steady. Some antennas cannot be matched on some bands (the
display then shows the best that was possible). Search effort 3 (setting 12)
tries harder.

**Tuning takes long.** A first tune usually takes 3 to 6 seconds (rarely up to
15), a tune on a band used before about 2 seconds. Search effort 1 is quicker but finds the best match
less often.

**The display stays dark.** Press the button once. If the display still does
not come on, switch the tuner off and on again.

## Development status

| Date | Version | Status | Content |
|---|---|---|---|
| 2026-10-02 | 0.1.0 | development | complete new firmware: search, memory of 12 tunes, setup menu, Cells editor; simulator and PC tests |
| 2026-10-02 | 0.9.0 | tested on the device | first device tests passed; review fixes, display clean-up after tuning |

Technical details: [How tuning works](docs/TUNING.md) · [Development](docs/DEVELOPMENT.md)

## Feedback

Reports from use on the air are very welcome – please describe the antenna,
band, power and what the display showed.

## License

Public domain ([Unlicense](LICENSE)). The ATU-10 hardware is a design by
David Fainitski, N7DDC.
