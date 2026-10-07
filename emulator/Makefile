# TMS5220 reference emulator (written from the reference datasheet only).
CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra
AR ?= ar

LIBOBJS = tms5220.o tms6100.o

all: tms5220emu libtms5220.a

libtms5220.a: $(LIBOBJS)
	$(AR) rcs $@ $(LIBOBJS)

tms5220emu: tms5220emu.o libtms5220.a
	$(CC) $(CFLAGS) -o $@ tms5220emu.o libtms5220.a

test_tms5220: test_tms5220.o libtms5220.a
	$(CC) $(CFLAGS) -o $@ test_tms5220.o libtms5220.a

%.o: %.c tms5220.h tms6100.h
	$(CC) $(CFLAGS) -c $<

test: test_tms5220 tms5220emu
	./test_tms5220

clean:
	rm -f *.o libtms5220.a tms5220emu test_tms5220

.PHONY: all test clean
