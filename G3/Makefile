CFLAGS=-O2 -g -Wall -pedantic -I../include

OBJECTS =	fsm.o initialize.o kernel.o commandframes.o

all: g3.a

g3.a: $(OBJECTS)
	$(AR) rcs $@ $^

clean:
	rm -f $(OBJECTS) g3.a *~

