CFLAGS=-ggdb -Wall -pedantic -ansi -I../include

OBJECTS =	globals.o readconfig.o watchdog.o environment.o \
		regmodules.o malloc.o isdnline.o timers.o softsignals.o \
		statemachine.o pty.o hardware-driver.o iobuffer.o

all: misc.a test

misc.a: $(OBJECTS)
	$(AR) rcs $@ $^

test:	$(OBJECTS) misc.a
	$(CC) $(CFLAGS) -o test test.c misc.a ../lib/alaw.o ../lib/debug.o

%.o:	%.c
	$(CC) $(CFLAGS) -c $^
clean:
	rm -f $(OBJECTS) misc.a *~ test
