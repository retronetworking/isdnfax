CFLAGS=-O2 -g -Wall -pedantic -I../include

OBJECTS =	globals.o readconfig.o watchdog.o environment.o \
		regmodules.o malloc.o isdnline.o

all: misc.a

misc.a: $(OBJECTS)
	$(AR) rcs $@ $^

isdnline:  isdnline.c
	cc -g isdnline.c -o isdnline -I../include -Wall -pedantic ../lib/alaw.c

testecho:  isdnline.c testecho.c
	cc -g							\
		isdnline.c					\
		../lib/alaw.c					\
		malloc.c					\
		testecho.c					\
		-o testecho -I../include -Wall -pedantic

clean:
	rm -f $(OBJECTS) misc.a *~
