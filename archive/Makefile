# Native Mac archive reader (hqx / sit / sit5 / MacBinary) and its arcdump tool.
CC ?= cc
CFLAGS += -O2 -Wall
all: tools/arcdump
tools/arcdump: tools/arcdump.c arc.c sit13.c arsenic.c arc.h sit13_tables.h
	$(CC) $(CFLAGS) -o $@ tools/arcdump.c arc.c sit13.c arsenic.c
clean:
	rm -f tools/arcdump
