// Renders screens of the setup menu with the firmware's own setup.c and
// display.c into PBM files (build/menu_*.pbm), for the documentation
#include <stdio.h>
#include <stdint.h>
#include <xc.h>

// stand-ins for the hardware (tests/host/xc.h)
struct PORTBbits_t PORTBbits; struct LATAbits_t LATAbits;
uint8_t ANSELA;
volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x10, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02 };
void fake_ms(uint32_t ms) { (void)ms; }
void fake_sleep(void) {}
void fake_clrwdt(void) {}
void delay_ms(uint16_t ms) { (void)ms; }
uint32_t tick_ms(void) { return 0; }
uint8_t oled_init(void) { return 0; }
uint8_t oled_on(void) { return 0; }
uint8_t oled_present(void) { return 1; }
uint8_t oled_write(uint8_t p, uint8_t x, const uint8_t *d, uint8_t n) { (void)p; (void)x; (void)d; (void)n; return 0; }
void i2c_init(void) {}
uint8_t buttons_event(void) { return 0; }
void buttons_clear(void) {}
void settings_save(void) {}
void settings_defaults(void) {}
void settings_load(void) {}

#include "../src/setup.c"

const uint8_t *disp_fb(void);

static void dump(const char *name) {
   char path[64];
   const uint8_t *fb = disp_fb();
   snprintf(path, sizeof path, "build/menu_%s.pbm", name);
   FILE *f = fopen(path, "w");
   fprintf(f, "P1\n128 32\n");
   for(int y = 0; y < 32; y++) {
      for(int x = 0; x < 128; x++) fprintf(f, "%d ", fb[(y / 8) * 128 + x] >> (y % 8) & 1);
      fputc('\n', f);
   }
   fclose(f);
}

int main(void) {
   cells_load();
   disp_clear();
   disp_big(LINE1, 34, "SETUP");
   dump("setup");
   show(2);                          // relay pulse, 10 ms
   dump("relay");
   cfg[CFG_RELAY_MS] = next_value(CFG_RELAY_MS, cfg[CFG_RELAY_MS]);
   show(2);                          // after a short press: 12 ms
   dump("relay2");
   cfg[CFG_LAYOUT] = 1;
   show(CFG_LAYOUT);                 // the main screen: relay view
   dump("display");
   show(PAGE_SAVE);
   dump("save");
   show(PAGE_HEX);
   dump("hex");
   show(PAGE_EXIT);
   dump("exit");
   return 0;
}
