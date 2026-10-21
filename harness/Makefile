# Mazeworld Catacombs 68k harness (Linux/macOS/MinGW)
MUSASHI = ../third_party/Musashi
GEN     = gen
CC     ?= cc
CFLAGS += -O2 -g -Wall -Wno-unused-parameter -I. -I$(GEN) -I$(MUSASHI) -DMUSASHI_CNF='"musashi_conf.h"'
MACTB   = ../mactb
OBJ     = main.o menu.o traps.o heap.o rsrc.o seg.o vfs.o qd.o dlg.o input.o vbl.o snd.o sub.o glue.o port_shell.o gen/subs.o game_seg1.o game_seg2.o game_seg2_frame.o game_seg3.o game_seg4.o game_seg7.o game_seg7_objects.o game_seg8.o game_seg8_tables.o game_seg8_ui.o game_seg8_loop.o game_seg8_shell.o game_seg8_help.o mactb_color.o mactb_pix8.o mactb_sane.o mactb_text.o mactb_font_data.o mactb_sound.o mactb_dialog.o mactb_pict.o arc_arc.o arc_sit13.o arc_arsenic.o $(GEN)/m68kops.o m68kcpu.o m68kdasm.o softfloat.o

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
game_%.o: $(GAME)/%.c $(GAME)/mz.h $(GAME)/mzasm.h $(GAME)/fns.h
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
