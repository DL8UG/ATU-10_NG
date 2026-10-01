#include "nvm.h"
#ifdef __XC8
#include <xc.h>
#endif

state_t st;

// The relay state changes with every tune and bypass toggle. To spread the
// wear (100k write cycles per byte) it goes into the next of 16 slots each
// time; a sequence number tells which slot is the newest.
#define RING_ADDR   0x60
#define RING_SLOTS  16
#define SLOT_SIZE   8          // seq, l, c|sw<<7, flags, byp l, byp c|sw<<7, last_swr, crc
#define MEM_ADDR    0x30       // magic, n, 12 x (l, c|sw<<7, swr), crc
#define MEM_MAGIC   0xA7

static uint8_t ring_slot, ring_seq;

#ifdef __XC8   // on the PC the unit test provides nvm_read / nvm_write
uint8_t nvm_read(uint8_t a) {
   while(NVMCON1bits.WR) continue;
   NVMCON1bits.NVMREGS = 1;    // data EEPROM at 0xF000
   NVMADRH = 0x70;
   NVMADRL = a;
   NVMCON1bits.RD = 1;
   return NVMDATL;
}

void nvm_write(uint8_t a, uint8_t v) {
   uint8_t gie;
   if(nvm_read(a) == v) return;
   NVMCON1bits.NVMREGS = 1;
   NVMADRH = 0x70;
   NVMADRL = a;
   NVMDATL = v;
   gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   NVMCON1bits.WREN = 1;
   NVMCON2 = 0x55;             // unlock sequence
   NVMCON2 = 0xAA;
   NVMCON1bits.WR = 1;
   INTCONbits.GIE = gie;
   while(NVMCON1bits.WR) CLRWDT();
   NVMCON1bits.WREN = 0;
}
#endif

// CRC-8 (polynomial 0x07); never 0 / 0xFF for erased cells in practice
uint8_t crc8(const uint8_t *p, uint8_t n) {
   uint8_t c = 0x5A, i;
   while(n--) {
      c ^= *p++;
      for(i = 0; i < 8; i++) c = (uint8_t)(c & 0x80 ? c << 1 ^ 0x07 : c << 1);
   }
   return c;
}

static uint8_t pack_c(const relays_t *r) {
   return (uint8_t)(r->c | (r->sw ? 0x80 : 0));
}

static void unpack(relays_t *r, uint8_t l, uint8_t csw) {
   r->l = l & 0x7F;
   r->c = csw & 0x7F;
   r->sw = csw >> 7;
}

static uint8_t slot_read(uint8_t s, uint8_t *b) {
   uint8_t i, a = (uint8_t)(RING_ADDR + s * SLOT_SIZE);
   for(i = 0; i < SLOT_SIZE; i++) b[i] = nvm_read((uint8_t)(a + i));
   return crc8(b, SLOT_SIZE - 1) == b[SLOT_SIZE - 1];
}

void state_save(void) {
   uint8_t b[SLOT_SIZE], i, a;
   ring_slot = (uint8_t)((ring_slot + 1) % RING_SLOTS);
   b[0] = ++ring_seq;
   b[1] = st.r.l;
   b[2] = pack_c(&st.r);
   b[3] = st.bypass;
   b[4] = st.byp.l;
   b[5] = pack_c(&st.byp);
   b[6] = st.last_swr;
   b[7] = crc8(b, SLOT_SIZE - 1);
   a = (uint8_t)(RING_ADDR + ring_slot * SLOT_SIZE);
   for(i = 0; i < SLOT_SIZE; i++) nvm_write((uint8_t)(a + i), b[i]);
}

// the newest valid slot: the highest sequence number, counted modulo 256
// (the valid slots are never more than 16 numbers apart)
uint8_t state_load(void) {
   uint8_t b[SLOT_SIZE], s, found = 0, best_seq = 0, best = 0;
   for(s = 0; s < RING_SLOTS; s++) {
      if(!slot_read(s, b)) continue;
      if(!found || (uint8_t)(b[0] - best_seq) < 128) {
         best_seq = b[0];
         best = s;
         found = 1;
      }
   }
   if(!found) {
      ring_slot = RING_SLOTS - 1;
      ring_seq = 0;
      return 0;
   }
   slot_read(best, b);
   ring_slot = best;
   ring_seq = b[0];
   unpack(&st.r, b[1], b[2]);
   st.bypass = b[3] ? 1 : 0;
   unpack(&st.byp, b[4], b[5]);
   st.last_swr = b[6];
   return 1;
}

void mem_save(void) {
   uint8_t b[2 + 3 * MEM_SLOTS], i;
   if(!tune_mem_changed) return;
   tune_mem_changed = 0;
   b[0] = MEM_MAGIC;
   b[1] = tune_mem_n;
   for(i = 0; i < MEM_SLOTS; i++) {
      b[2 + 3 * i] = tune_mem[i].l;
      b[3 + 3 * i] = pack_c(&tune_mem[i]);
      b[4 + 3 * i] = tune_mem_swr[i];
   }
   for(i = 0; i < sizeof b; i++) nvm_write((uint8_t)(MEM_ADDR + i), b[i]);
   nvm_write((uint8_t)(MEM_ADDR + sizeof b), crc8(b, sizeof b));
}

void mem_load(void) {
   uint8_t b[2 + 3 * MEM_SLOTS], i;
   tune_mem_n = 0;
   tune_mem_changed = 0;
   for(i = 0; i < sizeof b; i++) b[i] = nvm_read((uint8_t)(MEM_ADDR + i));
   if(b[0] != MEM_MAGIC || b[1] > MEM_SLOTS || crc8(b, sizeof b) != nvm_read((uint8_t)(MEM_ADDR + sizeof b)))
      return;
   for(i = 0; i < MEM_SLOTS; i++) {
      unpack(&tune_mem[i], b[2 + 3 * i], b[3 + 3 * i]);
      tune_mem_swr[i] = b[4 + 3 * i];
   }
   tune_mem_n = b[1];
}
