LDFLAGS=-lm

# Use this for development
CFLAGS=-ggdb -Wall -pedantic -ansi -I../include -Dinline=

#Use this for production
# CFLAGS=-O4 -Wall -pedantic -I../include

MODULES = send_to_audio.o pulsegen.o sinegen.o replicate.o \
	scrambler.o modulator-V29.o fsk_demod.o fsk_mod.o \
	decode_serial.o encode_serial.o debug.o rateconvert.o \
	decode_hdlc.o modulator-V21.o faxcontrol.o linedriver.o \
	signalgen.o V.29-demod.o hdlc-framing.o syncbit.o

HELPERS =

all: modules.a

modules.a: $(MODULES) $(HELPERS)
	$(AR) rcs $@ $^

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(MODULES) modules.a *~
