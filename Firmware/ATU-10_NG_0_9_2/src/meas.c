#include "board.h"
#include "meas.h"

// The detector voltages span from a few mV to the supply voltage, so the
// ADC reference is switched in three ranges:
//   FVR 1.024 V  1 mV per step
//   FVR 2.048 V  2 mV per step
//   Vdd          Vdd / 1024 per step (Vdd = battery voltage)
enum { REF_FVR1, REF_FVR2, REF_VDD };
#define RANGE_TOP 1000           // a sample above this moves to the next range

uint16_t vbat_mv = 4000;
static uint8_t ref_now = 0xFF;
static uint8_t ovf;

void meas_init(void) {
   ADCON0 = 0;
   ADCON1 = 0;
   ADCON2 = 0;                   // basic mode
   ADCON3 = 0;
   ADACQ = 0;                    // acquisition time is waited in software
   ADPRE = 0;
   ADCAP = 0;
   ADCON0 = 0b00010100;          // FRC clock, right justified
   ADCON0bits.ADON = 1;
   ref_now = 0xFF;
}

static void adc_ref(uint8_t ref) {
   if(ref == ref_now) return;
   ref_now = ref;
   if(ref == REF_VDD) ADREF = 0b00000000;
   else {
      FVRCONbits.ADFVR = ref == REF_FVR1 ? 0b01 : 0b10;
      FVRCONbits.FVREN = 1;
      while(!FVRCONbits.FVRRDY) continue;
      ADREF = 0b00000011;        // Vref+ = FVR
   }
   __delay_us(100);              // reference settles
}

// one conversion (a macro: saves a hardware stack level in the tuning)
#define ADC_READ(ch, res) do { \
   ADPCH = (ch); \
   __delay_us(20);               /* acquisition */ \
   ADCON0bits.ADGO = 1; \
   while(ADCON0bits.ADGO) continue; \
   (res) = (uint16_t)ADRESH << 8 | ADRESL; \
} while(0)

// Average of n samples of a detector in 1/8 mV. Starts in the most
// sensitive range and moves up when a sample does not fit.
static uint16_t detector(uint8_t ch, uint8_t n) {
   uint8_t ref, i;
   uint16_t s;
   uint32_t sum;
   for(ref = REF_FVR1; ; ref++) {
      adc_ref(ref);
      sum = 0;
      for(i = 0; i < n; i++) {
         ADC_READ(ch, s);
         if(s > RANGE_TOP && ref != REF_VDD) break;
         if(s >= 1023) ovf = 1;
         sum += s;
      }
      if(i == n) break;
   }
   switch(ref) {
      case REF_FVR1: sum *= Q_PER_MV; break;
      case REF_FVR2: sum *= 2 * Q_PER_MV; break;
      default:       sum = sum * vbat_mv / 1024 * Q_PER_MV; break;
   }
   return (uint16_t)((sum + n / 2) / n);
}

uint16_t meas_battery(void) {
   uint8_t i;
   uint16_t sum = 0, s;
   adc_ref(REF_FVR1);
   for(i = 0; i < 8; i++) {
      ADC_READ(ADC_CH_BAT, s);
      sum += s;
   }
   vbat_mv = (uint16_t)(((uint32_t)sum * 11 + 4) / 8);
   return vbat_mv;
}

void meas_take(meas_t *m, uint8_t n) {
   uint16_t f1, r1, r2, f2;
   CLRWDT();
   ovf = 0;
   f1 = detector(ADC_CH_FWD, n);
   r1 = detector(ADC_CH_REV, n);
   r2 = detector(ADC_CH_REV, n);
   f2 = detector(ADC_CH_FWD, n);
   meas_finish(m, f1, r1, r2, f2);
   m->overflow = ovf;
}
