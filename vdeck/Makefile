#	Makefile -- for vdeck

#	last edit:	86/06/17	G S Moss

#	SCCSID	@(#)Makefile	2.7

PRODUCT = vdeck
HEADERS = ${PRODUCT}.h ged_types.h 3d.h vextern.h
CFILES	= ${PRODUCT}.c cgarbs.c match.c parsarg.c vproc.c vglobal.c vsort.c
OBJS	= ${PRODUCT}.o cgarbs.o match.o parsarg.o vproc.o vglobal.o vsort.o
VMBLIB	= # /vld/lib/libVMB.a
LIBES	= ${VMBLIB} -lm
LDFLAGS = -n# -m 6 # NEED -m flag on GOULDS.
TFILES	= ${PRODUCT}.in ${PRODUCT}.exp
BINDIR	= /vld/bin
LIBDIR	= /vld/lib
INCDIR	= /vld/include
VAXMAN	= /usr/5lib/man/local/man1
PDPMAN	= /usr/man/s5/local/man1
TESTDIR = .
INS	= cp
RM	= /bin/rm
GET	= get
SHELL	= /usr/5bin/sh

.DEFAULT:
	$(GET) $(GFLAGS) -p s.$@ > ${TESTDIR}/$@
	touch $@

.c.o:
	$(CC) -I${INCDIR} $(CFLAGS) -c $<

.c~.o:
	$(GET) $(GFLAGS) -p $< > $*.c
	$(CC) $(CFLAGS) -c -I${INCDIR} $<
	-rm -f $*.c

all:		${PRODUCT} ${PRODUCT}.1

${PRODUCT}:	${OBJS}
	$(CC) -o ${TESTDIR}/$@ ${LDFLAGS} ${OBJS} ${LIBES} 
	size ${TESTDIR}/$@
	touch $@

${OBJS}:	${HEADERS}

print:		${PRODUCT}.1 Makefile ${HEADERS} ${CFILES}
	( nroff -Tlp -man ${TESTDIR}/${PRODUCT}.1 ; \
	  pr Makefile ${HEADERS} ${CFILES} \
				${TESTDIR}/${PRODUCT}.1 ) | lpr

lint:		${HEADERS} ${CFILES}
	lint -I${INCDIR} ${CFILES} ${LIBES} > ${TESTDIR}/${PRODUCT}.lint

flow:		${HEADERS} ${CFILES}
	cflow ${CFILES} > ${TESTDIR}/${PRODUCT}.flow

xref:		${HEADERS} ${CFILES}
	cxref -c -s -w132 ${CFILES} > ${TESTDIR}/${PRODUCT}.xref

test:		${PRODUCT} # ${TFILES}
	${TESTDIR}/${PRODUCT} < ${PRODUCT}.in \
		> ${TESTDIR}/${PRODUCT}.out 2> ${TESTDIR}/${PRODUCT}.err
	@if cmp ${PRODUCT}.exp ${TESTDIR}/${PRODUCT}.out ; \
	then	echo 'Tested okay' ; \
	else	echo 'Test failed; differences:' ; \
		diff ${PRODUCT}.exp ${TESTDIR}/${PRODUCT}.out ; \
		exit 1 ; \
	fi

compare:	all # test
	cmp ${BINDIR}/${PRODUCT} ${TESTDIR}/${PRODUCT}
	cmp ${MANDIR}/${PRODUCT}.1 ${TESTDIR}/${PRODUCT}.1

install:	all # test
	${INS} ${TESTDIR}/${PRODUCT} ${BINDIR}
	chmod 775 ${BINDIR}/${PRODUCT}
	-if vax || gould; then \
		${INS} ${TESTDIR}/${PRODUCT}.1 ${VAXMAN}/${PRODUCT}.1; \
		chmod 664 ${VAXMAN}/${PRODUCT}.1; \
	else \
		${INS} ${TESTDIR}/${PRODUCT}.1 ${PDPMAN}/${PRODUCT}.1; \
		chmod 644 ${PDPMAN}/${PRODUCT}.1; \
	fi

clean:
	-if vax; then rm -f ${HEADERS} ${CFILES} ; fi
	-rm -f ${OBJS} ${TESTDIR}/${PRODUCT}.lint \
		${TESTDIR}/${PRODUCT}.flow ${TESTDIR}/${PRODUCT}.xref \
		${TESTDIR}/${PRODUCT}.out ${TESTDIR}/${PRODUCT}.err

clobber:	clean
	-if vax; then rm -f ${TESTDIR}/${PRODUCT}.1; fi
	-rm -f ${TESTDIR}/${PRODUCT}
