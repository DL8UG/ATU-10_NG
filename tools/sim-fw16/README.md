# FW 1.6 search in the NG simulator

The tuning functions of FW 1.6 (N7DDC), built for the simulator of NG, so
that both search algorithms can be compared on the same cases. The results
are on the page [Comparison with FW 1.6](../../docs/COMPARISON.md).

## Contents

- [Files](#files)
- [Running the comparison](#running-the-comparison)

## Files

| File | Content |
|---|---|
| `tune.c` | `get_swr`, `tune`, `subtune`, `coarse_*`, `sharp_*`, `atu_reset` – copied unchanged from `main.c` of FW 1.6; only the call of `Btn_short()` on a button press is removed |
| `swr.c` | the power and SWR calculation of `get_pwr()` and `sqrt_n()`, unchanged; the display handling is left out |
| `tune.h`, `swr.h` | the interface to the simulator |
| `compare.sh` | builds both simulators and runs the comparison |

The relays and the ADC readings (with FW 1.6's three ranges) come from the
simulator: `tools/sim/glue_ref.c` of the version folder. This code is never
built for the tuner. FW 1.6 by N7DDC is public domain.

## Running the comparison

From the top of the repository:

```
tools/sim-fw16/compare.sh                              # newest version folder
tools/sim-fw16/compare.sh Firmware/ATU-10_NG_0_9_5     # a given one
```

It needs a C compiler (`cc`, or set `CC`) and Python 3, and takes a few
minutes. It builds `fw16` (this code) and `ng` (the search of the version
folder) into `build/` here and runs both on the same cases:

- ideal measurement, 3 mV ADC noise, and the hard scenario (noise, unsteady
  carrier, QRP rig with 10 Ohm source resistance, component tolerance,
  detector calibration off) – the scenarios of the version folder's
  Makefile
- a small QSY (1 to 3 %) after a tune, and band changes

For each it prints the table of `tools/sim/compare.py` with both side by
side, then how often SWR 1.20 was reached where possible, and how many tunes
took longer than 20 s.
