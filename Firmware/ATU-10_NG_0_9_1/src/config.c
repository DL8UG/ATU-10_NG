// Configuration words of the PIC16LF18877.
//
// The tuner is updated by its on-board USB programmer (PIC16F1454), which
// writes the hex file through ICSP in low-voltage mode. To keep that path
// open whatever the firmware does, these bits must never change:
//   LVP = ON, MCLRE = ON  - the programmer holds the PIC in reset via MCLR
//   CP = OFF, CPD = OFF   - no read protection
//   WRT = OFF             - no write protection
// The Makefile checks them in the finished hex file.
//
// Other settings:
// - brown-out reset at 2.7 V while running (off in sleep: no extra current),
//   an almost empty battery resets the PIC cleanly instead of letting it run
//   out of spec at 32 MHz
// - watchdog of about 8 s, switched on by software after the start and off
//   in sleep, so a hang resets the tuner

#include <xc.h>

#pragma config FEXTOSC = OFF, RSTOSC = HFINT32, CLKOUTEN = OFF, CSWEN = ON, FCMEN = ON
#pragma config MCLRE = ON, PWRTE = ON, LPBOREN = OFF, BOREN = NSLEEP, BORV = HI
#pragma config ZCD = OFF, PPS1WAY = ON, STVREN = ON, DEBUG = OFF
#pragma config WDTCPS = WDTCPS_13, WDTE = SWDTEN, WDTCWS = WDTCWS_7, WDTCCS = LFINTOSC
#pragma config WRT = OFF, SCANE = available, LVP = ON
#pragma config CP = OFF, CPD = OFF
