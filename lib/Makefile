LDFLAGS=-lm
CFLAGS=-O2 -ggdb -Wall -pedantic -ansi -I../include

LIBOBJS = bitreverse.o debug.o int2alaw.o module.o sincos.o g711.o \
	  rate-7k2-8k-1.o atan.o atantbl.o sqrt.o sqrttbl.o alaw.o \
	  rate-8k-7k2-1.o

all: isdnlib.a

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

isdnlib.a: $(LIBOBJS)
	$(AR) rcs $@ $^

clean:
	rm -f $(LIBOBJS) isdnlib.a *~
