LDFLAGS=-lm
CFLAGS=-O2 -g -Wall -pedantic -Iinclude

SUBDIRS = lib modules misc

MODULES = modules/modules.a lib/isdnlib.a misc/misc.a

PROGRAMS = v21_softmodem test amodemd

all: subdirs $(PROGRAMS)

subdirs:
	for a in $(SUBDIRS) ; do make -C $$a ; done

v21_softmodem:	v21_softmodem.o $(OBJECTS) $(MODULES)
	$(CC) -o $@ $^ $(LDFLAGS) 

test:	test.o $(OBJECTS) $(MODULES)
	$(CC) -o $@ $^ $(LDFLAGS)

amodemd:	amodemd.o $(MODULES)
		$(CC) -o $@ $^ $(LDFLAGS) 

misc/misc.a: misc/*.o
	make -C misc

clean:
	rm -f $(OBJECTS) $(MODULES) $(PROGRAMS) *~ *.o
	for a in $(SUBDIRS) ; do make -C $$a clean; done
