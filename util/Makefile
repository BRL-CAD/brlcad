
#	Makefile -- for Frame buffer utilities

#	last edit:	86/06/18	Gary S. Moss

#	SCCS ID:	@(#)Makefile	1.22

PRODUCTS= pp-fb fbclear fbgrid rle-fb fb-rle fbcmap
HEADERS =
S52_CFILES= pp-fb.c fbclear.c fbgrid.c rle-fb.c fb-rle.c fbcmap.c fbzoom.c\
	termio.c
BSD_CFILES= pp-fb.c fbclear.c fbgrid.c rle-fb.c fb-rle.c fbcmap.c fbzoom.c\
	termio.c getopt.c strchr.c
CFILES	= ${S52_CFILES}
S52_OBJS= pp-fb.o fbclear.o fbgrid.o rle-fb.o fb-rle.o fbcmap.o fbzoom.o\
	termio.o
BSD_OBJS= pp-fb.o fbclear.o fbgrid.o rle-fb.o fb-rle.o fbcmap.o fbzoom.o\
	termio.o getopt.o strchr.o
OBJS	= ${S52_OBJS}
BINDIR	= /vld/bin
LIBDIR	= /vld/lib
S52_INCDIR=/vld/include
BSD_INCDIR=/d/mike/cad/dist/h
INCDIR	= ${S52_INCDIR}
MANDIR	= /usr/5lib/man/local/man1
DISTDIR	= /d/mike/cad/dist/fb_utils
MANUALS	= pp-fb.1 fbclear.1 fbgrid.1 rle-fb.1 fb-rle.1 fbcmap.1 fbzoom.1
BINPERM	= 775
MANPERM	= 664
S52_CFLAGS = -I${INCDIR} -O
BSD_CFLAGS = -I${INCDIR} -O -DBSD
CFLAGS	= ${S52_CFLAGS}
LDFLAGS = -n
S52_LIBES= ${LIBDIR}/librle.a ${LIBDIR}/libfb.a -lm
BSD_LIBES= -lrle -lfb -lm
LIBES	= ${S52_LIBES}
SOURCES	= Makefile make_bsd ${CFILES} ${HEADERS} ${MANUALS}
COMPAT=
INS	= cp
SHELL	= /usr/5bin/sh

.DEFAULT:
	$(GET) $(GFLAGS) -p s.$@ > $@
	touch $@

all:	fbzoom ${PRODUCTS} ${MANUALS}

${PRODUCTS}:	$$@.o ${COMPAT}
	$(CC) -o $@ ${LDFLAGS} $@.o ${COMPAT} ${LIBES}
	size $@
	touch $@

fbzoom: fbzoom.o termio.o ${COMPAT}
	$(CC) -o $@ ${LDFLAGS} $@.o ${COMPAT} termio.o ${LIBES} 
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
	@for i in ${PRODUCTS} fbzoom ; \
	do	if cmp -s ${BINDIR}/$$i $$i; \
		then	echo $$i unchanged; \
		else	${INS} $$i ${BINDIR} && echo $$i installed in ${BINDIR}; \
			chmod ${BINPERM} ${BINDIR}/$$i ; \
		fi \
	done
	@for i in ${MANUALS} ; \
	do	if cmp -s ${MANDIR}/$$i $$i; \
		then	echo $$i unchanged; \
		else	${INS} $$i ${MANDIR} && echo $$i installed in ${MANDIR}; \
			chmod ${MANPERM} ${MANDIR}/$$i ; \
		fi \
	done
	-@if vax; \
	 then ${INS} ${SOURCES} ${DISTDIR} \
		&& echo sources installed in ${DISTDIR}; \
	 fi

clean:
	-rm -f ${OBJS}

clobber:	clean
	-rm -f ${PRODUCTS} fbzoom

pp-fb.o: pp-fb.c
fbclear.o: fbclear.c
fbgrid.o: fbgrid.c
rle-fb.o: rle-fb.c
fb-rle.o: fb-rle.c
fbcmap.o: fbcmap.c
fbzoom.o: fbzoom.c
termio.o: termio.c
getopt.o: getopt.c
strchr.o: strchr.c
