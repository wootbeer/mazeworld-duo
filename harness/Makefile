# Mazeworld Catacombs 68k harness (Linux/macOS/MinGW)
MUSASHI = ../third_party/Musashi
GEN     = gen
CC     ?= cc
CFLAGS += -O2 -g -Wall -Wno-unused-parameter -I. -I$(GEN) -I$(MUSASHI) -DMUSASHI_CNF='"musashi_conf.h"'
MACTB   = ../mactb
OBJ     = main.o menu.o traps.o heap.o rsrc.o seg.o vfs.o qd.o dlg.o input.o vbl.o snd.o sub.o glue.o port_shell.o gen/subs.o game_seg1.o game_seg2.o game_seg2_frame.o game_seg3.o game_seg4.o game_seg7.o game_seg7_objects.o game_seg8.o game_seg8_tables.o game_seg8_ui.o game_seg8_loop.o game_seg8_shell.o game_seg8_help.o game_games.o game_orig_seg2.o game_orig_seg7.o game_orig_seg8.o game_orig_objects.o game_orig_shell.o game_orig_creatures.o mactb_color.o mactb_pix8.o mactb_sane.o mactb_text.o mactb_font_data.o mactb_sound.o mactb_dialog.o mactb_pict.o arc_arc.o arc_sit13.o arc_arsenic.o $(GEN)/m68kops.o m68kcpu.o m68kdasm.o softfloat.o

all: mzharness

$(GEN)/m68kmake: $(MUSASHI)/m68kmake.c
	@mkdir -p $(GEN)
	$(CC) -O2 -o $@ $<

$(GEN)/m68kops.c $(GEN)/m68kops.h: $(GEN)/m68kmake $(MUSASHI)/m68k_in.c
	$(GEN)/m68kmake $(GEN) $(MUSASHI)/m68k_in.c

m68kcpu.o: $(MUSASHI)/m68kcpu.c $(GEN)/m68kops.h musashi_conf.h
	$(CC) $(CFLAGS) -w -c -o $@ $<
m68kdasm.o: $(MUSASHI)/m68kdasm.c musashi_conf.h
	$(CC) $(CFLAGS) -w -c -o $@ $<
softfloat.o: $(MUSASHI)/softfloat/softfloat.c
	$(CC) $(CFLAGS) -w -c -o $@ $<
$(GEN)/m68kops.o: $(GEN)/m68kops.c musashi_conf.h
	$(CC) $(CFLAGS) -w -c -o $@ $<

GAME    = ../game
game_%.o: $(GAME)/%.c $(GAME)/mz.h $(GAME)/mzasm.h $(GAME)/fns.h $(GAME)/orig_map.h
	$(CC) $(CFLAGS) -DMZ_HARNESS -I$(GAME) -c -o $@ $<

gen/subs.c $(GAME)/fns.h: $(GAME)/subs.txt ../tools/gen_subs.py
	python3 ../tools/gen_subs.py

gen/subs.o: gen/subs.c hx.h $(GAME)/mzasm.h
	$(CC) $(CFLAGS) -c -o $@ $<

port_shell.o: ../port/shell.c ../port/shell.h $(GAME)/mz.h $(GAME)/fns.h
	$(CC) $(CFLAGS) -I$(GAME) -c -o $@ $<

arc_%.o: ../archive/%.c ../archive/arc.h
	$(CC) $(CFLAGS) -c -o $@ $<

mactb_%.o: $(MACTB)/%.c $(MACTB)/mactb.h $(MACTB)/font_data.h
	$(CC) $(CFLAGS) -I$(MUSASHI) -c -o $@ $<

%.o: %.c hx.h qd_int.h ../game/mz.h $(GEN)/trapsigs.h $(GEN)/m68kops.h
	$(CC) $(CFLAGS) -c -o $@ $<

mzharness: $(OBJ)
	$(CC) -o $@ $(OBJ) -lm

clean:
	rm -f *.o $(GEN)/*.o $(GEN)/m68kops.* $(GEN)/m68kmake mzharness

# ---------- the playable test build (mzplay): the rig in a window, recording what you play ----------
PLAYOBJ = $(filter-out main.o,$(OBJ)) main_play.o play.o
main_play.o: main.c hx.h $(GEN)/m68kops.h
	$(CC) $(CFLAGS) -DHX_PLAY -c -o $@ $<
play.o: play.c hx.h ../port/sdlkeys.h
	$(CC) $(CFLAGS) $(shell sdl2-config --cflags) -c -o $@ $<
mzplay: $(PLAYOBJ)
	$(CC) -o $@ $(PLAYOBJ) $(shell sdl2-config --libs) -lm

# Windows (MinGW, SDL2 from third_party/sdl2-mingw): mzplay.exe
WCC     ?= x86_64-w64-mingw32-gcc
SDLW     = ../third_party/sdl2-mingw/x86_64-w64-mingw32
WB       = wbuild
WFLAGS   = -O2 -g0 -Wno-unused-parameter -I. -I$(GEN) -I$(MUSASHI) -DMUSASHI_CNF='"musashi_conf.h"' -I$(SDLW)/include/SDL2
WOBJ     = $(addprefix $(WB)/,$(PLAYOBJ))
$(WB)/%.o: %.c ; @mkdir -p $(WB) $(WB)/gen; $(WCC) $(WFLAGS) -c -o $@ $<
$(WB)/gen/%.o: gen/%.c ; @mkdir -p $(WB)/gen; $(WCC) $(WFLAGS) -w -c -o $@ $<
$(WB)/game_%.o: $(GAME)/%.c $(GAME)/orig_map.h ; @mkdir -p $(WB); $(WCC) $(WFLAGS) -DMZ_HARNESS -I$(GAME) -c -o $@ $<
$(WB)/arc_%.o: ../archive/%.c ; @mkdir -p $(WB); $(WCC) $(WFLAGS) -c -o $@ $<
$(WB)/mactb_%.o: $(MACTB)/%.c ; @mkdir -p $(WB); $(WCC) $(WFLAGS) -c -o $@ $<
$(WB)/m68kcpu.o: $(MUSASHI)/m68kcpu.c $(GEN)/m68kops.h ; @mkdir -p $(WB); $(WCC) $(WFLAGS) -w -c -o $@ $<
$(WB)/m68kdasm.o: $(MUSASHI)/m68kdasm.c ; @mkdir -p $(WB); $(WCC) $(WFLAGS) -w -c -o $@ $<
$(WB)/softfloat.o: $(MUSASHI)/softfloat/softfloat.c ; @mkdir -p $(WB); $(WCC) $(WFLAGS) -w -c -o $@ $<
$(WB)/main_play.o: main.c ; @mkdir -p $(WB); $(WCC) $(WFLAGS) -DHX_PLAY -Dmain=SDL_main -c -o $@ $<
$(WB)/port_shell.o: ../port/shell.c ; @mkdir -p $(WB); $(WCC) $(WFLAGS) -I$(GAME) -c -o $@ $<
mzplay.exe: $(WOBJ)
	$(WCC) -o $@ $(WOBJ) -static -L$(SDLW)/lib -lmingw32 -lSDL2main -lSDL2 -mconsole -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lhid
