#	Makefile -- for vdeck

#	last edit:	86/07/10	G S Moss

#	SCCSID	@(#)Makefile	2.8

PRODUCT = vdeck
HEADERS = ${PRODUCT}.h ged_types.h 3d.h vextern.h
CFILES	= ${PRODUCT}.c cgarbs.c match.c parsarg.c vproc.c vglobal.c vsort.c
OBJS	= ${PRODUCT}.o cgarbs.o match.o parsarg.o vproc.o vglobal.o vsort.o
VMBLIB	= /vld/lib/libVMB.a
LIBES	= -lm
LDFLAGS = -n# -m 6 # NEED -m flag on GOULDS.
TFILES	= ${PRODUCT}.in ${PRODUCT}.exp
BINDIR	= /vld/bin
LIBDIR	= /vld/lib
INCDIR	= /vld/include
MANDIR	= /usr/5lib/man/local/man1
INS	= cp
RM	= /bin/rm
GET	= get
SHELL	= /usr/5bin/sh

.DEFAULT:
	$(GET) $(GFLAGS) -p s.$@ > $@
	touch $@

.c.o:
	$(CC) -I${INCDIR} $(CFLAGS) -c $<

.c~.o:
	$(GET) $(GFLAGS) -p $< > $*.c
	$(CC) $(CFLAGS) -c -I${INCDIR} $<
	-rm -f $*.c

all:		${PRODUCT} ${PRODUCT}.1

${PRODUCT}:	${OBJS}
	$(CC) -o $@ ${LDFLAGS} ${OBJS} ${LIBES} 
	size $@
	touch $@

${OBJS}:	${HEADERS}

print:		${PRODUCT}.1 Makefile ${HEADERS} ${CFILES}
	( nroff -Tlp -man ${PRODUCT}.1 ; \
	  pr Makefile ${HEADERS} ${CFILES} \
				${PRODUCT}.1 ) | lpr

lint:		${HEADERS} ${CFILES}
	lint -I${INCDIR} ${CFILES} ${LIBES} > ${PRODUCT}.lint

flow:		${HEADERS} ${CFILES}
	cflow ${CFILES} > ${PRODUCT}.flow

xref:		${HEADERS} ${CFILES}
	cxref -c -s -w132 ${CFILES} > ${PRODUCT}.xref

test:		${PRODUCT} # ${TFILES}
	${PRODUCT} < ${PRODUCT}.in \
		> ${PRODUCT}.out 2> ${PRODUCT}.err
	@if cmp ${PRODUCT}.exp ${PRODUCT}.out ; \
	then	echo 'Tested okay' ; \
	else	echo 'Test failed; differences:' ; \
		diff ${PRODUCT}.exp ${PRODUCT}.out ; \
		exit 1 ; \
	fi

compare:	all
	cmp ${BINDIR}/${PRODUCT} ${PRODUCT}
	cmp ${MANDIR}/${PRODUCT}.1 ${PRODUCT}.1

install:	all
	@if cmp -s ${PRODUCT} ${BINDIR}/${PRODUCT}; \
	 then	echo "${PRODUCT} unchanged."; \
	 else	${INS} ${PRODUCT} ${BINDIR} && \
		echo "${PRODUCT} installed in ${BINDIR}."; \
		chmod 775 ${BINDIR}/${PRODUCT}; \
	 fi
	@if cmp -s ${PRODUCT}.1 ${MANDIR}/${PRODUCT}.1; \
	 then	echo "${PRODUCT}.1 unchanged."; \
	 else	${INS} ${PRODUCT}.1 ${MANDIR}/${PRODUCT}.1 && \
		echo "${PRODUCT}.1 installed in ${MANDIR}."; \
		chmod 664 ${MANDIR}/${PRODUCT}.1; \
	 fi

clean:
	-rm -f ${OBJS} ${PRODUCT}.lint \
		${PRODUCT}.flow ${PRODUCT}.xref \
		${PRODUCT}.out ${PRODUCT}.err

clobber:	clean
	-rm -f ${PRODUCT}
