#
#			db/Makefile
# This Makefile only used for preparing the distribution!
# We assume that the .g files are fine, and merely work on the .asc ones.
#
.SUFFIXES: .g .asc

PRODUCTS	= bldg391.asc crod.asc demo.asc ktank.asc moss.asc prim.asc \
		  star.asc truck.asc world.asc cube.asc axis.asc sphflake.asc

SHELL		= /bin/sh

DISTDIR		= /m/dist/db

##.asc.g: ; $(A2G) < $? > $@

A2G		= asc2g
G2A		= g2asc

all:		${PRODUCTS}

dist:		${PRODUCTS}

bldg391.asc:	bldg391.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
crod.asc:	crod.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
demo.asc:	demo.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
ktank.asc:	ktank.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
moss.asc:	moss.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
prim.asc:	prim.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
star.asc:	star.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
truck.asc:	truck.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
world.asc:	world.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
cube.asc:	cube.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
axis.asc:	axis.g
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc
sphflake.asc:
	-mv -f $*.asc $*.asc.bak; $(G2A) < $? > $*.asc ; chmod 444 $*.asc

depend:
	echo "db: depend"

install: ${PRODUCTS}
	-@sh -c "if test `hostname` = brl-vgr ; then make inst-dist; else exit 0 ; fi"

inst-dist: ${PRODUCTS}
	-rm -fr ${DISTDIR}/*
	-cp Makefile.dist ${DISTDIR}/Makefile
	-@for prod in ${PRODUCTS} ; \
	do	if cmp -s ${DISTDIR}/$$prod $$prod; \
		 then	echo $$prod unchanged; \
		 else	rm -f ${DISTDIR}/$$prod  2>/dev/null; \
			cp $$prod ${DISTDIR} && \
			echo "++++++INSTALLED" ${DISTDIR}/$$prod && \
			chmod 444 ${DISTDIR}/$$prod; \
		 fi \
	done

clean:
	echo "db: cleaned"

clobber:
	echo "db: clobber ignored"
