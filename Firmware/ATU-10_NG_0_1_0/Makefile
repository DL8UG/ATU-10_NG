# ATU-10 NG firmware, built with Microchip XC8
#   make          -> ATU-10_NG_0_1_0.hex (copy it onto the tuner's USB drive)
#   make test     -> host unit tests
#   make clean    (keeps the committed hex file)

VERSION = 0_1_0
TARGET  = ATU-10_NG_$(VERSION)

XC8    ?= $(firstword $(wildcard /opt/microchip/xc8/*/bin/xc8-cc) xc8-cc)
CPU     = 16LF18877
# Device Family Pack from https://packs.download.microchip.com (XC8 v3+ ships without it)
DFP    ?= $(lastword $(wildcard $(HOME)/.local/share/microchip/packs/PIC16F1xxxx_DFP/*))
SRC     = $(addprefix src/, app.c board.c cells.c config.c timer.c)
HDR     = $(wildcard src/*.h)
# must match the #pragma config values in src/config.c
CONFIG  = 2904,3CA1,072D,3003,0003
# hardware stack: 16 levels; keep a margin for the interrupt and surprises
MAX_STACK = 11

CFLAGS  = -mcpu=$(CPU) -mdfp=$(DFP)/xc8 -std=c99 -O2 -Isrc

all: $(TARGET).hex

build/fw.hex: $(SRC) $(HDR)
	mkdir -p build
	$(XC8) $(CFLAGS) -Wa,-a -Wl,-Map=build/fw.map -o build/fw.elf $(SRC)
	@depth=$$(sed -n 's/.*Estimated maximum stack depth \([0-9]*\).*/\1/p' build/fw.lst | sort -n | tail -1); \
	 echo "hardware stack depth incl. interrupt: $$depth of 16 (limit $(MAX_STACK))"; \
	 test -n "$$depth" && test $$depth -le $(MAX_STACK)

$(TARGET).hex: build/fw.hex tools/normalize_hex.py tools/cells.py
	python3 tools/normalize_hex.py $< $@ --config $(CONFIG)
	python3 tools/normalize_hex.py --check $@
	python3 tools/cells.py check $@ --defaults

clean:
	rm -rf build

.PHONY: all clean test

# ---- host unit tests (gcc)
HOSTCC   = gcc
HOSTFLAGS = -O2 -std=c99 -Wall -Wextra -D_DEFAULT_SOURCE -Isrc
TESTS    = cells

build/test_%: tests/test_%.c tests/check.h $(HDR) $(wildcard src/*.c)
	mkdir -p build
	$(HOSTCC) $(HOSTFLAGS) -o $@ $< -lm

test: $(addprefix build/test_, $(TESTS)) $(TARGET).hex
	@for t in $(TESTS); do build/test_$$t || exit 1; done
	sh tests/test_tools.sh $(TARGET).hex
