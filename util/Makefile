#	Makefile -- for Frame buffer utilities

#	last edit:	85/03/28	Gary S. Moss

#	SCCS ID:	@(#)Makefile	1.11

PRODUCTS= pp-fb fbclear fbgrid rle-fb fb-rle fbcmap
HEADERS =
CFILES	= pp-fb.c fbclear.c fbgrid.c rle-fb.c fb-rle.c fbcmap.c fbzoom.c\
	termio.c
OBJS	= pp-fb.o fbclear.o fbgrid.o rle-fb.o fb-rle.o fbcmap.o fbzoom.o\
	termio.o
#/usr/brl/bin eventually
BINDIR	= /vld/bin
LIBDIR	= /vld/lib
INCDIR	= /vld/include
MANDIR	= /usr/man/man1
MANUALS	= pp-fb.1 fbclear.1 fbgrid.1 rle-fb.1 fb-rle.1 fbcmap.1 fbzoom.1
BINPERM	= 755
MANPERM	= 664
CFLAGS	= -I${INCDIR} -O
LDFLAGS = -n
LIBES	= ${LIBDIR}/librle.a ${LIBDIR}/libfb.a -lm
INS	= cp

.DEFAULT:
	$(GET) $(GFLAGS) -p s.$@ > $@
	touch $@

all:	fbzoom ${PRODUCTS}# ${MANUALS}

${PRODUCTS}:	$$@.o
	$(CC) -o $@ ${LDFLAGS} $@.o ${LIBES} 
	size $@
	touch $@

fbzoom: fbzoom.o termio.o
	$(CC) -o $@ ${LDFLAGS} $@.o termio.o ${LIBES} 
	size $@
	touch $@

print:		${MANUALS} Makefile ${HEADERS} ${CFILES}
	( nroff -Tlp -man ${MANUALS} ; \
	  pr Makefile ${HEADERS} ${CFILES} \
				${MANUALS} ) | lpr

lint:		${HEADERS} ${CFILES}
	lint -I${INCDIR} ${CFILES} > ${PRODUCTS}.lint

flow:		${HEADERS} ${CFILES}
	cflow ${CFILES} > ${PRODUCTS}.flow

xref:		${HEADERS} ${CFILES}
	cxref -c -s -w132 ${CFILES} > ${PRODUCTS}.xref

compare:	all
	for i in ${PRODUCTS} fbzoom ; \
	do	cmp ${BINDIR}/$$i $$i ; \
	done
	for i in ${MANUALS} ; \
	do	cmp $$i ${MANDIR}/$$i ; \
	done

install:	all
	${INS} ${PRODUCTS} fbzoom ${BINDIR}
	for i in ${PRODUCTS} fbzoom ; \
	do	chmod ${BINPERM} ${BINDIR}/$$i ; \
	done
#	for i in ${MANUALS} ; \
#	do	${INS} $$i ${MANDIR}/$$i ; \
#		chmod ${MANPERM} ${MANDIR}/$$i ; \
#	done

clean:
	-if vax; then rm -f ${HEADERS} ${CFILES}; fi
	-rm -f ${OBJS}

clobber:	clean
	-if vax; then rm -f ${MANUALS}; fi
	-rm -f ${PRODUCTS} fbzoom

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
