// Battery thresholds of the battery voltage vbat_mv (meas.c), in mV.

#ifndef BATTERY_H
#define BATTERY_H

// Battery levels (app.c); BATT_OFF_MV is also the empty battery symbol (display.c)
#define BATT_WARN_MV 3400                     // below: the battery symbol blinks
#define BATT_LOW_MV  3200                     // below: RECHARGE (tuning still works)
#define BATT_OFF_MV  3000                     // below: LOW BATT, switched off
// The LED blink at each battery check (app.c): green, yellow, else red
#define BATT_GREEN_MV  3700                   // above: green
#define BATT_YELLOW_MV 3590                   // above: yellow (both)
// Relay pulse 1 ms longer (relays.c): a weak battery drives the coils slower
#define BATT_SLOW_MV 3800                     // at and below

#endif
