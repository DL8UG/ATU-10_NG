// Effective settings: the Cells of the hex file, or the values saved by
// the setup menu in the EEPROM. The saved values only apply while the hex
// Cells are still the ones they were saved with (hash): flashing a hex
// file with other Cells brings its values into effect.

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

void settings_load(void);            // cfg[] from the hex Cells, then the EEPROM block
void settings_save(void);            // cfg[] -> EEPROM block
void settings_defaults(void);        // delete the block, cfg[] from the hex Cells

#endif
