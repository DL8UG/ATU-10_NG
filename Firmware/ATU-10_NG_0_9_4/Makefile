# ATU-10 NG firmware, built with Microchip XC8
#   make          -> ATU-10_NG_0_9_4.hex (copy it onto the tuner's USB drive)
#   make test     -> host unit tests
#   make clean    (keeps the committed hex file)

VERSION = 0_9_4
TARGET  = ATU-10_NG_$(VERSION)

XC8    ?= $(firstword $(wildcard /opt/microchip/xc8/*/bin/xc8-cc) xc8-cc)
CPU     = 16LF18877
# Device Family Pack from https://packs.download.microchip.com (XC8 v3+ ships without it)
DFP    ?= $(lastword $(wildcard $(HOME)/.local/share/microchip/packs/PIC16F1xxxx_DFP/*))
SRC     = $(addprefix src/, app.c board.c buttons.c cells.c config.c display.c i2c_soft.c \
             meas.c meas_math.c nvm.c oled.c relays.c settings.c setup.c text.c timer.c tune.c)
HDR     = $(wildcard src/*.h)
# must match the #pragma config values in src/config.c
CONFIG  = 2904,3CA1,072D,3003,0003
# hardware stack: 16 levels; keep a margin (the interrupt is included)
MAX_STACK = 10

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

.PHONY: all clean test sim simcompare simretune docs

# ---- host unit tests (gcc)
HOSTCC   = gcc
HOSTFLAGS = -O2 -std=c99 -Wall -Wextra -D_DEFAULT_SOURCE -Isrc
TESTS    = cells meas antennas nvm display settings tune

build/test_%: tests/test_%.c tests/check.h $(HDR) $(wildcard src/*.c tools/sim/*)
	mkdir -p build
	$(HOSTCC) $(HOSTFLAGS) -Itools/sim -o $@ $< -lm

# the main program with simulated hardware and time (tests/host/xc.h)
APP_HOST = $(addprefix src/, timer.c buttons.c cells.c settings.c nvm.c tune.c meas_math.c display.c setup.c text.c)
build/test_app: tests/test_app.c tests/host/xc.h $(HDR) $(wildcard src/*.c)
	mkdir -p build
	$(HOSTCC) $(HOSTFLAGS) -Wno-unused-parameter -Itests/host -Dmain=app_main -c src/app.c -o build/app_host.o
	$(HOSTCC) $(HOSTFLAGS) -Wno-unused-parameter -Itests/host -o $@ tests/test_app.c build/app_host.o $(APP_HOST) -lm

test: $(addprefix build/test_, $(TESTS)) build/test_app $(TARGET).hex
	@for t in $(TESTS) app; do build/test_$$t || exit 1; done
	@for v in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19; do build/test_app $$v || exit 1; done
	sh tests/test_tools.sh $(TARGET).hex

# ---- PC simulator of the tuning algorithm (tools/sim, see sim.c)
SIM_SRC  = $(addprefix tools/sim/, sim.c model.c antennas.c)
SIM_FW   = src/tune.c src/meas_math.c src/cells.c
SIMFLAGS = -O2 -std=c99 -Wall -D_DEFAULT_SOURCE $(SIMDEF)

build/sim: $(SIM_SRC) tools/sim/glue_new.c tools/sim/model.h $(SIM_FW) $(HDR)
	mkdir -p build
	$(HOSTCC) $(SIMFLAGS) -Isrc -Itools/sim -o $@ $(SIM_SRC) tools/sim/glue_new.c $(SIM_FW) -lm

# Development only: an older algorithm for comparison, from outside this
# repository (tune.c / swr.c of the previous firmware line)
REF_DIR ?= $(HOME)/Work/ATU-10-DL8UG/Firmware/ATU-10_FW_182_xc8
build/sim_ref: $(SIM_SRC) tools/sim/glue_ref.c tools/sim/model.h
	mkdir -p build
	$(HOSTCC) $(SIMFLAGS) -w -funsigned-char -I$(REF_DIR) -Itools/sim -o $@ \
	   $(SIM_SRC) tools/sim/glue_ref.c $(REF_DIR)/tune.c $(REF_DIR)/swr.c -lm

sim: build/sim

# Scenarios: ideal, ADC noise, and a hard one (noise, a QRP rig that is no
# 50 Ohm source, an unsteady carrier, 5 % component tolerance, detector
# calibration off)
SEEDS   ?= 1 2 3 4 5
SC_clean = 
SC_noise = --noise 3
SC_hard  = --noise 3 --rs 10 --jitter 0.03 --tol 5 --cal 1.3 0.3
SIMOPT  ?=
HAVE_REF = $(wildcard $(REF_DIR)/tune.c)

build/new_%.tsv: build/sim tools/sim/run.sh
	sh tools/sim/run.sh build/sim $@ "$(if $(filter clean,$*),1,$(SEEDS))" $(SC_$*) $(SIMOPT)

build/ref_%.tsv: build/sim_ref tools/sim/run.sh
	sh tools/sim/run.sh build/sim_ref $@ "$(if $(filter clean,$*),1,$(SEEDS))" $(SC_$*)

# new algorithm in all scenarios; side by side with the reference if REF_DIR exists
simcompare: $(foreach s,clean noise hard,build/new_$(s).tsv $(if $(HAVE_REF),build/ref_$(s).tsv))
	@for s in clean noise hard; do echo "== $$s"; \
	   python3 tools/sim/compare.py $(if $(HAVE_REF),build/ref_$$s.tsv) build/new_$$s.tsv; done

# QSY of a few percent after a tune: second tune (quick retune)
simretune: build/sim
	for p in -3 -1 1 3; do build/sim --suite ant --retune $$p --noise 3; done > build/new_retune.tsv
	python3 tools/sim/compare.py build/new_retune.tsv

# ---- charts and pictures for the documentation (docs/ at the top of the repository)
DOCS = ../../docs
docs: build/sim build/test_display
	mkdir -p $(DOCS)
	build/sim --ant 5 3.65 --noise 3 --map build/map.tsv 2> /dev/null > /dev/null
	python3 tools/sim/plot.py landscape build/map.tsv $(DOCS)/search-landscape.svg \
	   "Random wire 25.6 m with 9:1 unun, 3.65 MHz"
	sh tools/sim/run.sh build/sim build/doc_cold.tsv "1 2 3" --noise 3
	build/sim --hop 12 --noise 3 > build/doc_hop.tsv
	python3 tools/sim/plot.py times build/doc_cold.tsv build/doc_hop.tsv $(DOCS)/tuning-time.svg
	build/test_display > /dev/null
	python3 tools/sim/plot.py screen build/screen_main.pbm $(DOCS)/display-main.png
	python3 tools/sim/plot.py screen build/screen_greeting.pbm $(DOCS)/display-greeting.png
	python3 tools/sim/plot.py screen build/screen_greeting2.pbm $(DOCS)/display-greeting2.png
	$(HOSTCC) -O2 -std=c99 -D_DEFAULT_SOURCE -Isrc -Itests/host -o build/render_menu tests/render_menu.c \
	   src/display.c src/text.c src/cells.c
	build/render_menu
	for m in setup relay relay2 save hex exit; do \
	   python3 tools/sim/plot.py screen build/menu_$$m.pbm $(DOCS)/menu-$$m.png; done
