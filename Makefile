LDFLAGS=-lm
CFLAGS=-O2 -g -Wall -pedantic -Iinclude

SUBDIRS = lib modules

MODULES = modules/modules.a lib/isdnlib.a

PROGRAMS = v21_softmodem

all: subdirs $(PROGRAMS)

subdirs:
	for a in $(SUBDIRS) ; do make -C $$a ; done

v21_softmodem:	v21_softmodem.o $(OBJECTS) $(MODULES)
	$(CC) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f $(OBJECTS) $(MODULES) $(PROGRAMS) *~ *.o
	for a in $(SUBDIRS) ; do make -C $$a clean; done
