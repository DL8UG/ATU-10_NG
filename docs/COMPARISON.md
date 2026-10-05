# Comparison with FW 1.6

This page compares ATU-10 NG with FW 1.6, the original firmware of the ATU-10
by N7DDC: the settings, how both search for a match, and how well both tune.
The tuning results come from the simulator, where both search algorithms ran
against the same models of antennas and of the tuner.

> This comparison is purely technical. It is not meant to belittle FW 1.6
> or the work of N7DDC: the ATU-10 and its original firmware are the basis
> NG is built on. The two firmwares also aim at different things – FW 1.6
> at a quick tune that stops at SWR 1.20, NG at the best possible match,
> taking longer for it.

## Contents

- [In short](#in-short)
- [Settings and behaviour](#settings-and-behaviour)
- [The search](#the-search)
  - [FW 1.6](#fw-16)
  - [ATU-10 NG](#atu-10-ng)
- [How the comparison was made](#how-the-comparison-was-made)
- [Results](#results)
  - [By scenario](#by-scenario)
  - [By antenna type](#by-antenna-type)
  - [What the numbers say](#what-the-numbers-say)

## In short

- In the simulator, NG reaches SWR 1.5 or better in practically every case
  where that is possible at all (99.2 to 100 %); FW 1.6 in about 61 %.
- FW 1.6 stops at SWR 1.20. Even that target it reaches in only 52 % of the
  cases where it is possible; NG in 99.8 %.
- FW 1.6 is faster (about 1.5 s against 5.8 s for a first tune), because it
  stops early, often on the slope of a valley. Back on a band used before,
  NG needs about 2 to 3 s.
- With a QRP rig that is no 50 Ohm source, FW 1.6 waited for minutes in the
  model in about 3 of 10 cases (on average almost 2 minutes) and then gave
  up; NG did not.

## Settings and behaviour

Both use the defaults of their hex files.

| | FW 1.6 | ATU-10 NG 0.9.5 |
|---|---|---|
| Settings (Cells) | 10, only in the hex file | 12, in a menu on the tuner, in a browser editor or in the hex file |
| Display off / power off after | 5 / 30 min | 5 / 30 min |
| Relay pulse | 7 ms (8 ms below 3.8 V) | 10 ms, as the relay data sheets ask |
| Min. / max. tune power | 1.0 W / 15 W, maximum checked on the forward power | 1.0 W / 15 W, maximum checked on the power the transmitter delivers (forward − reflected) |
| Calibration a / b | 1.14 / 0.4 | 1.14 / 0.4 |
| Power peak hold | 600 ms | 600 ms |
| **Tune target** | fixed: stop at SWR 1.20 | **SWR 1.05** (Cell 11; 0 = always the full search) |
| Search effort | – | Cell 12: quick, normal, thorough |
| What the search compares | SWR, capped at 9.99; the coarse search in steps of 0.1 | Pr / Pf, not capped |
| One measurement | single ADC readings; of up to 5 in a row the lowest SWR counts | average of many samples, with an estimate of its noise |
| Coarse search | L and C one after the other, single relays 1, 2, 4 … 32, stops at the first step that is worse | grid of 5 x 5 L and C values for both capacitor sides (50 settings) |
| Fine search | L and C one after the other, steps of about 10 %, once per capacitor side | from the 3 best grid points: pattern search, valley moves, capacitor to the other side |
| Comparison with bypass | none | final check: the result is set only if it is clearly better than bypass, else bypass or NO MATCH |
| Memory | none | the last 12 good tunes, quick retune from them |
| Auto tune | SWR above 1.20 and changed by more than 0.3 | the same, and only after 4 steady measurements in a row and not within 3 s after a tune |
| Detector above its range | "OVERLOAD" shown, the tune goes on | 64 such settings in a row stop the tune (OVERLOAD) |
| Battery: switched off at | 3.4 V | 3.0 V (symbol blinks below 3.4 V, RECHARGE below 3.2 V) |

## The search

### FW 1.6

1. Measure the current setting; done if the SWR is 1.20 or better.
2. With the capacitor on one side: from L = C = 0, add capacitor relays one by
   one (22, 47, 100 … pF) as long as the SWR does not get worse, then coil
   relays the same way. If both end at the smallest relays, try the other
   order and L and C together, and keep the best of these.
3. Fine search: change the larger of L and C, then the other one, in steps
   of about a tenth of the value, as long as the SWR does not get worse.
4. The same with the capacitor on the other side; keep the better side and
   search finely once more.

Between these steps the search ends as soon as the SWR is 1.20 or better.
Each search direction stops
at the first step that is not better. The good settings lie in narrow,
curved valleys (see [How tuning works](TUNING.md#why-tuning-is-hard)), so
such a search often stops on the slope of a valley, or in a small valley
next to the good one.

### ATU-10 NG

The search is described in detail in [How tuning works](TUNING.md#the-search):
the remembered results first, then a coarse grid over the whole range, a
careful local search from the 3 best places, and a final check against
bypass. A setting only counts as better if it is better by more than the
noise of the measurement.

## How the comparison was made

The simulator of NG (`tools/sim`) models the L
network, the bridge, the detector diodes, the ADC and the transmitter, and
behind it real antennas: random wires with a 9:1 unun, an EFHW, resonant and
non-resonant dipoles, dipoles with a 1:4 balun, doublets on ladder line, and
126 fixed loads, each on all bands from 160 to 10 m. For every case it also
tries all 32768 relay settings, to know the best possible SWR.

Both search algorithms ran in this simulator against exactly the same cases:

- **NG**: the search and measurement code of the firmware itself (`tune.c`,
  `meas_math.c`), as in the [results on the tuning page](TUNING.md#results-in-the-simulator).
- **FW 1.6**: the tuning functions copied unchanged from `main.c` of FW 1.6
  (`get_swr`, `tune`, `subtune`, `coarse_tune` and the coarse and fine
  searches, the power and SWR calculation with its `sqrt_n`). Only the
  hardware access was replaced by the model: the relays, and the ADC
  readings with FW 1.6's three ranges (1.024 V, 2.048 V, battery voltage).
  The display and button code in these functions was left out.

Both use their default settings (FW 1.6: relay pulse 7 ms, target 1.20; NG:
10 ms, target 1.05). The tuning time is estimated the same way for both: 3
relay pulses plus settling time per relay setting, plus the ADC readings.

The FW 1.6 code for the simulator and a script that runs the whole
comparison are in [`tools/sim-fw16`](../tools/sim-fw16/README.md).

This is a simulation, not a measurement on the device. It shows how the two
search algorithms behave on the same task; the absolute numbers depend on
the models.

## Results

### By scenario

"Matchable" means that the best possible setting reaches SWR 1.5 or better;
the percentages refer to these cases.

| Scenario | Matchable cases | reached SWR ≤ 1.5<br>FW 1.6 / NG | within 0.05 of the best possible<br>FW 1.6 / NG | mean tuning time<br>FW 1.6 / NG |
|---|---|---|---|---|
| ideal measurement | 768 | 62.8 % / **100 %** | 45.3 % / **100 %** | 1.5 s / 5.7 s |
| 3 mV ADC noise | 3840 | 61.2 % / **100 %** | 43.7 % / **100 %** | 1.5 s / 5.8 s |
| hard: noise, unsteady carrier (3 %), QRP rig with 10 Ohm source resistance, 5 % component tolerance, detector calibration off | 3860 | 52.7 % / **99.2 %** | 38.4 % / **99.0 %** | 34.9 s ¹ / 6.4 s |
| small QSY (1 to 3 %) after a tune | 2652 | 62.6 % / **100 %** | 44.9 % / **99.8 %** | 1.5 s / 2.9 s |
| band changes (20 ↔ 30 m, 40 ↔ 20 m, ...) | 2008 | 64.4 % / **100 %** | 40.5 % / **100 %** | 1.4 s / 2.8 s ² |

¹ In 1374 of 4680 cases FW 1.6 waited up to about 150 s and then gave up
without a result: at a strong mismatch the QRP rig (5 W) drives the forward
power above 15 W, and FW 1.6 waits for the forward power to come down below
its maximum tune power. NG checks the power the transmitter delivers instead.
The other cases took 3.2 s on average.

² NG with its memory filled from the earlier tunes; FW 1.6 has no memory and
searches anew each time.

FW 1.6's own target, SWR 1.20 (3 mV noise, cases where 1.20 is possible):
FW 1.6 reached it in 52.2 %, NG in 99.8 % of 2605 cases.

Case by case (3 mV noise): NG ended better in 2943 of 4680 cases, worse in
6 and equal (within 0.02) in 1731.

### By antenna type

3 mV ADC noise, matchable cases.

| Antenna type | reached SWR ≤ 1.5<br>FW 1.6 / NG | within 0.05 of the best possible<br>FW 1.6 / NG |
|---|---|---|
| random wire with 9:1 unun | 66.3 % / 100 % | 47.7 % / 100 % |
| EFHW for 40 m with 49:1 transformer | 78.8 % / 100 % | 35.4 % / 100 % |
| resonant dipoles on coax | 73.7 % / 100 % | 56.6 % / 100 % |
| dipoles with 1:4 balun at the feed point | 64.6 % / 100 % | 38.5 % / 100 % |
| non-resonant dipoles on coax | 65.0 % / 100 % | 55.2 % / 100 % |
| doublets on ladder line with 1:4 balun | 30.3 % / 100 % | 25.9 % / 100 % |
| fixed loads 5 to 2000 Ohm | 53.5 % / 99.8 % | 34.7 % / 99.8 % |

### What the numbers say

- **Match:** the main difference. FW 1.6 misses a usable match in about
  four of ten cases where one exists – also with ideal measurements, so the
  cause is the search, not the noise. Doublets on ladder line, whose
  impedance varies most from band to band, are the hardest for it.
- **Time:** FW 1.6 is faster because it tries fewer settings and stops at
  SWR 1.20 or at the first step that is not better. NG takes about 4 s
  longer for a first tune; back on a band it used before, about 1.5 s
  longer.
- **Power check:** with a QRP rig at a strong mismatch, FW 1.6 can wait a
  long time for "good power" that does not come.
