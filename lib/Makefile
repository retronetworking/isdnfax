LDFLAGS=-lm
CFLAGS=-O2 -g -Wall -pedantic -I../include

LIBOBJS = bitreverse.o debug.o int2alaw.o module.o sincos.o isdn.o g711.o \
	  rate-7k2-8k-1.o

all: isdnlib.a

isdnlib.a: $(LIBOBJS)
	$(AR) rcs $@ $^

clean:
	rm -f $(LIBOBJS) *~
