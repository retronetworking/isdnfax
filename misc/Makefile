CFLAGS=-O2 -g -Wall -pedantic -I../include

OBJECTS =	globals.o readconfig.o watchdog.o environment.o \
		regmodules.o malloc.o isdnline.o timers.o softsignals.o \
		statemachine.o pty.o

all: misc.a

misc.a: $(OBJECTS)
	$(AR) rcs $@ $^

clean:
	rm -f $(OBJECTS) misc.a *~
