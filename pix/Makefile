#
#			pix/Makefile
# This version used only to make the distribution
#
.SUFFIXES: .pix .p_asc

PRODUCTS	= moss.p_asc star.p_asc bldg391.p_asc world.p_asc \
		  moss.log star.log bldg391.log world.log \
		  m109.p_asc sphflake.log

DISTDIR		= /m/dist/pix

#.p_asc.pix: ; $(A2P) < $? > $@

A2P		= asc2pix
P2A		= pix2asc

all:	moss.p_asc world.p_asc star.p_asc bldg391.p_asc

bldg391.p_asc:	bldg391.pix
	$(P2A) < $? > $*.p_asc ; chmod 444 $*.p_asc
moss.p_asc:	moss.pix
	$(P2A) < $? > $*.p_asc ; chmod 444 $*.p_asc
world.p_asc:	world.pix
	$(P2A) < $? > $*.p_asc ; chmod 444 $*.p_asc
star.p_asc:	star.pix
	$(P2A) < $? > $*.p_asc ; chmod 444 $*.p_asc
m109.p_asc:	m109.pix
	$(P2A) < $? > $*.p_asc ; chmod 444 $*.p_asc
sphflake.p_asc:	sphflake.pix
	$(P2A) < $? > $*.p_asc ; chmod 444 $*.p_asc

depend:
	echo "pix:  depend"

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
	echo "pix: cleaned"

clobber:
	echo "pix: clobber ignored"

SHELL		= /bin/sh
