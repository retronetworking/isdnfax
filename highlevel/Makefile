CFLAGS=-O2 -g -Wall -pedantic -I../include

MODULES = commandparse.o

all: highlevel.a

highlevel.a: $(MODULES)
	$(AR) rcs $@ $^

clean:
	rm -f $(MODULES) ctrl.a *~
