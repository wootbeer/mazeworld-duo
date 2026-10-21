# The native port. `make` builds the headless test build for this machine (mzport-headless);
# `make win` cross-compiles the Windows game with MinGW and SDL2.
CC      ?= cc
H        = ../harness
GAME     = ../game
MACTB    = ../mactb
MUSASHI  = ../third_party/Musashi
CFLAGS  += -O2 -g -Wall -Wno-unused-parameter -I. -I$(H) -I$(H)/gen -I$(MUSASHI) -I$(GAME) -DMUSASHI_CNF='"musashi_conf.h"' -DMZ_PORT
# the rig's Mac (Toolbox) modules, without the CPU, the checker or the rig's main and VBL interrupt
TB       = menu traps heap rsrc seg vfs qd dlg input snd glue
GAMESRC  = seg1 seg2 seg2_frame seg3 seg4 seg7 seg7_objects seg8 seg8_tables seg8_ui seg8_loop seg8_shell seg8_help
MACTBSRC = color pix8 sane text font_data sound dialog pict
ARCSRC   = arc sit13 arsenic

define objs
$(addprefix $(1)/tb_,$(addsuffix .o,$(TB))) $(addprefix $(1)/game_,$(addsuffix .o,$(GAMESRC))) \
$(addprefix $(1)/mactb_,$(addsuffix .o,$(MACTBSRC))) $(addprefix $(1)/arc_,$(addsuffix .o,$(ARCSRC))) \
$(1)/rt.o $(1)/vbl.o $(1)/shell.o $(1)/softfloat.o $(1)/subs.o
endef

all: mzport-headless mzport

# the C game's function list and the rig's glue are generated from game/subs.txt
$(GAME)/fns.h $(H)/gen/subs.c: $(GAME)/subs.txt ../tools/gen_subs.py
	cd .. && python3 tools/gen_subs.py

B = build
$(B)/tb_%.o: $(H)/%.c $(H)/hx.h ; @mkdir -p $(B); $(CC) $(CFLAGS) -c -o $@ $<
$(B)/game_%.o: $(GAME)/%.c $(GAME)/mz.h ; @mkdir -p $(B); $(CC) $(CFLAGS) -c -o $@ $<
$(B)/mactb_%.o: $(MACTB)/%.c ; @mkdir -p $(B); $(CC) $(CFLAGS) -c -o $@ $<
$(B)/arc_%.o: ../archive/%.c ; @mkdir -p $(B); $(CC) $(CFLAGS) -c -o $@ $<
$(B)/softfloat.o: $(MUSASHI)/softfloat/softfloat.c ; @mkdir -p $(B); $(CC) $(CFLAGS) -w -c -o $@ $<
$(B)/subs.o: $(H)/gen/subs.c ; @mkdir -p $(B); $(CC) $(CFLAGS) -c -o $@ $<
$(B)/%.o: %.c port.h shell.h ; @mkdir -p $(B); $(CC) $(CFLAGS) -c -o $@ $<
$(B)/sdl_main.o: sdl_main.c port.h shell.h ; @mkdir -p $(B); $(CC) $(CFLAGS) $(shell sdl2-config --cflags) -c -o $@ $<

mzport-headless: $(call objs,$(B)) $(B)/headless.o
	$(CC) -o $@ $^ -lm

mzport: $(call objs,$(B)) $(B)/sdl_main.o
	$(CC) -o $@ $^ $(shell sdl2-config --libs) -lm

# Windows (MinGW cross-compiler and the SDL2 MinGW development package)
WCC     ?= x86_64-w64-mingw32-gcc
WINDRES ?= x86_64-w64-mingw32-windres
SDLW     = ../third_party/sdl2-mingw/x86_64-w64-mingw32
W        = build-win
WFLAGS   = -O2 -Wall -Wno-unused-parameter -I. -I$(H) -I$(H)/gen -I$(MUSASHI) -I$(GAME) -DMUSASHI_CNF='"musashi_conf.h"' -DMZ_PORT -I$(SDLW)/include/SDL2 -Dmain=SDL_main
$(W)/tb_%.o: $(H)/%.c $(H)/hx.h ; @mkdir -p $(W); $(WCC) $(WFLAGS) -c -o $@ $<
$(W)/game_%.o: $(GAME)/%.c $(GAME)/mz.h ; @mkdir -p $(W); $(WCC) $(WFLAGS) -c -o $@ $<
$(W)/mactb_%.o: $(MACTB)/%.c ; @mkdir -p $(W); $(WCC) $(WFLAGS) -c -o $@ $<
$(W)/arc_%.o: ../archive/%.c ; @mkdir -p $(W); $(WCC) $(WFLAGS) -c -o $@ $<
$(W)/softfloat.o: $(MUSASHI)/softfloat/softfloat.c ; @mkdir -p $(W); $(WCC) $(WFLAGS) -w -c -o $@ $<
$(W)/subs.o: $(H)/gen/subs.c ; @mkdir -p $(W); $(WCC) $(WFLAGS) -c -o $@ $<
$(W)/%.o: %.c port.h shell.h ; @mkdir -p $(W); $(WCC) $(WFLAGS) -c -o $@ $<
$(W)/headless.o: headless.c port.h ; @mkdir -p $(W); $(WCC) $(WFLAGS) -Umain -c -o $@ $<

win: Mazeworld.exe mzport-headless.exe
mzport-headless.exe: $(call objs,$(W)) $(W)/headless.o
	$(WCC) -o $@ $^ -static -lm
$(W)/mazeworld_rc.o: mazeworld.rc ; @mkdir -p $(W); $(WINDRES) $< -o $@
Mazeworld.exe: $(call objs,$(W)) $(W)/sdl_main.o $(W)/mazeworld_rc.o
	$(WCC) -o $@ $^ -static -L$(SDLW)/lib -lmingw32 -lSDL2main -lSDL2 -mwindows -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lcomdlg32 -lsetupapi -lversion -luuid -lhid

clean:
	rm -rf build build-win mzport-headless *.exe
