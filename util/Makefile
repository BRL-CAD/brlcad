#	Makefile -- for Frame buffer utilities

#	last edit:	85/03/12	Gary S. Moss

#	SCCS ID:	@(#)Makefile	1.2

PRODUCT	= pp-ik pc-ik ikclear ikgrid rle-ik ik-rle ikcmap ikzoom
HEADERS =
CFILES	= pp-ik.c pc-ik.c ikclear.c ikgrid.c rle-ik.c ik-rle.c\
	ikcmap.c ikzoom.c
OBJS	= pp-ik.o pc-ik.o ikclear.o ikgrid.o rle-ik.o ik-rle.o\
	ikcmap.o ikzoom.o
TFILES	= ${PRODUCT}.in ${PRODUCT}.exp
BINDIR	= /usr/brl/bin
LIBDIR	= /vld/lib
INCDIR	= /vld/include
MANDIR	= /usr/man/man1
MANUAL	= ik-rle.1
BINPERM	= 755
MANPERM	= 664
CFLAGS	= -I${INCDIR} -O
LDFLAGS = -n
LIBES	= ${LIBDIR}/libfb.a -lm
INS	= cp

.DEFAULT:
	$(GET) $(GFLAGS) -p s.$@ > ${TESTDIR}/$@
	touch $@

all:		${PRODUCT} ${MANUAL}

${PRODUCT}:	$$@.o
	$(CC) -o $@ ${LDFLAGS} $@.o ${LIBES} 
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
	${INS} ${PRODUCT} ${BINDIR}
	chmod ${BINPERM} ${BINDIR}/${PRODUCT}
	${INS} ${MANUAL} ${MANDIR}/${MANUAL}
	chmod ${MANPERM} ${MANDIR}/${MANUAL}

clean:
	-if vax; then rm -f ${HEADERS} ${CFILES} ${TFILES}; fi
	-rm -f ${OBJS} ${PRODUCT}.lint \
		${PRODUCT}.flow ${PRODUCT}.xref \
		${PRODUCT}.out ${PRODUCT}.err

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
# This line will be replaced by your file dependencies.
