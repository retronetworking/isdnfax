LDFLAGS=-lm
CFLAGS=-O2 -g -Wall -pedantic -I../include

MODULES = send_to_audio.o pulsegen.o sinegen.o replicate.o \
	mod_scrambler.o modulator-V29.o fsk_demod.o fsk_mod.o \
	decode_serial.o encode_serial.o debug.o

HELPERS = scrambler17.o

all: modules.a

modules.a: $(MODULES) $(HELPERS)
	$(AR) rcs $@ $^

clean:
	rm -f $(MODULES) *~
