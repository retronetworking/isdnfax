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

dist:
	(build_dir=/tmp;						\
	fax_dir=i4lfax-`date +%Y%m%d`;					\
	rm -rf $${build_dir}/$${fax_dir};				\
	mkdir $${build_dir}/$${fax_dir};				\
	tar cf - . | (cd $${build_dir}/$${fax_dir}; tar xf - );		\
	cd $${build_dir}/$${fax_dir};					\
	rm -rf html;							\
	find . -name CVS -type d -exec rm -rf {} \; -prune;		\
	find . -name .cvsignore -exec rm {} \; ;			\
	find . -name \*~ -exec rm {} \;	;				\
	find . -name \#\*\# -exec rm {} ;				\
	find . -name \*.o -exec rm {} \; ;				\
	find . -name \*.a -exec rm {} \; ;				\
	rm -f test amodemd v21_softmodem;				\
	cd $${build_dir};						\
	tar cfz $${fax_dir}.tar.gz $${fax_dir};				\
	rm -rf $${fax_dir};						\
	)
