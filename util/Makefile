#	Makefile -- for Frame buffer utilities

#	last edit:	85/03/14	Gary S. Moss

#	SCCS ID:	@(#)Makefile	1.8

PRODUCT	= pp-fb fbclear fbgrid rle-fb fb-rle fbcmap
HEADERS =
CFILES	= pp-fb.c fbclear.c fbgrid.c rle-fb.c fb-rle.c fbcmap.c fbzoom.c\
	termio.c
OBJS	= pp-fb.o fbclear.o fbgrid.o rle-fb.o fb-rle.o fbcmap.o fbzoom.o\
	termio.o
#/usr/brl/bin eventually
BINDIR	= /vld/moss/bin
LIBDIR	= /vld/lib
INCDIR	= /vld/include
MANDIR	= /usr/man/man1
MANUAL	= fb-rle.1
BINPERM	= 755
MANPERM	= 664
CFLAGS	= -I${INCDIR} -O
LDFLAGS = -n
LIBES	= ${LIBDIR}/libfb.a -lm
INS	= cp

.DEFAULT:
	$(GET) $(GFLAGS) -p s.$@ > $@
	touch $@

all:	fbzoom ${PRODUCT} ${MANUAL}

${PRODUCT}:	$$@.o
	$(CC) -o $@ ${LDFLAGS} $@.o ${LIBES} 
	size $@
	touch $@

fbzoom: fbzoom.o termio.o
	$(CC) -o $@ ${LDFLAGS} $@.o termio.o ${LIBES} 
	size $@
	touch $@

print:		${MANUAL} Makefile ${HEADERS} ${CFILES}
	( nroff -Tlp -man ${MANUAL} ; \
	  pr Makefile ${HEADERS} ${CFILES} \
				${MANUAL} ) | lpr

lint:		${HEADERS} ${CFILES}
	lint -I${INCDIR} ${CFILES} > ${PRODUCT}.lint

flow:		${HEADERS} ${CFILES}
	cflow ${CFILES} > ${PRODUCT}.flow

xref:		${HEADERS} ${CFILES}
	cxref -c -s -w132 ${CFILES} > ${PRODUCT}.xref

compare:	all
	cmp ${BINDIR}/${PRODUCT} ${PRODUCT}
	cmp ${MANDIR}/${MANUAL} ${MANUAL}

install:	all
	${INS} ${PRODUCT} fbzoom ${BINDIR}
	for i in ${PRODUCT} fbzoom ; \
	do	chmod ${BINPERM} $$i ; \
	done
#	${INS} ${MANUAL} ${MANDIR}/${MANUAL}
#	chmod ${MANPERM} ${MANDIR}/${MANUAL}

clean:
	-if vax; then rm -f ${HEADERS} ${CFILES}; fi
	-rm -f ${OBJS}

clobber:	clean
	-if vax; then rm -f ${MANUAL}; fi
	-rm -f ${PRODUCT}

# Richard Draves <draves@HARVARD.ARPA> provide this depend implementation.
BSD_CPP	=	/lib/cpp
ED	=	/bin/ed

depend	: ${CFILES} ${HEADERS}
	{ echo '/^# DO NOT DELETE THIS LINE/+1,$$c';	\
	  for i in $(CFILES); do $(BSD_CPP) -M -I${INCDIR} -I/usr/5include $$i; done;	\
	  echo '.';							\
	  echo 'w';							\
	} | $(ED) - Makefile

# File Relationships
# DO NOT DELETE THIS LINE -- make depends on it.
pp-fb.o: pp-fb.c
pp-fb.o: /usr/5include/stdio.h
pp-fb.o: /vld/include/fb.h
fbclear.o: fbclear.c
fbclear.o: /usr/5include/stdio.h
fbclear.o: /vld/include/std.h
fbclear.o: /vld/include/fb.h
fbgrid.o: fbgrid.c
fbgrid.o: /usr/5include/stdio.h
fbgrid.o: /vld/include/fb.h
rle-fb.o: rle-fb.c
rle-fb.o: /usr/5include/stdio.h
rle-fb.o: /usr/5include/string.h
rle-fb.o: /vld/include/fb.h
fb-rle.o: fb-rle.c
fb-rle.o: /usr/5include/stdio.h
fb-rle.o: /vld/include/fb.h
fbcmap.o: fbcmap.c
fbcmap.o: /usr/5include/stdio.h
fbcmap.o: /vld/include/fb.h
fbzoom.o: fbzoom.c
fbzoom.o: /usr/5include/stdio.h
fbzoom.o: /vld/include/fb.h
termio.o: termio.c
termio.o: /usr/5include/stdio.h
termio.o: /usr/5include/sys/ioctl.h
termio.o: /usr/5include/fcntl.h
termio.o: /usr/5include/termio.h
termio.o: /usr/5include/sys/termio.h
termio.o: /usr/5include/sys/_ioctl.h
termio.o: /usr/5include/memory.h
