LDFLAGS=-lm
CFLAGS=-O2 -g -Wall -pedantic -Iinclude

SUBDIRS = lib misc modules G3

MODULES = misc/misc.a modules/modules.a G3/g3.a lib/isdnlib.a

PROGRAMS = v21_softmodem amodemd #test

all: subdirs $(PROGRAMS)

subdirs:
	for a in $(SUBDIRS) ; do make -C $$a ; done

v21_softmodem:	v21_softmodem.o $(OBJECTS) $(MODULES)
	$(CC) -o $@ $^ $(MODULES) $(LDFLAGS) 

test:	test.o $(OBJECTS) $(MODULES)
	$(CC) -o $@ $^ $(MODULES) $(LDFLAGS)

amodemd:	amodemd.o $(MODULES)
		$(CC) -o $@ $^ $(MODULES)  $(LDFLAGS)



clean:
	rm -f $(OBJECTS) $(MODULES) $(PROGRAMS) *~ *.o
	for a in $(SUBDIRS) ; do make -C $$a clean; done
