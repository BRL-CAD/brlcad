# $Id$

SUBDIRS = include

A2G=$(top_builddir)/src/conv/asc2g
G2A=$(top_builddir)/src/conv/g2asc

# the shell script fu is to cope with gmake vs irix make weirdness...
# gmake seems to prefix the src with the appropriate srcdir automagically...
.asc.g:
	@if test -x $(A2G) ; then \
		echo $(A2G) $< $@ ; \
		$(A2G) `if test -e $< ; then echo $< ; else echo $(top_srcdir)/db/$< ; fi` $@ ; \
	else \
		echo "$(A2G) does not exist" ; \
	fi

MODELS = \
	axis.asc \
	bldg391.asc \
	boolean-ops.asc \
	castle.asc \
	cornell.asc \
	cray.asc \
	crod.asc \
	cube.asc \
	demo.asc \
	galileo.asc \
	havoc.asc \
	kman.asc \
	ktank.asc \
	lgt-test.asc \
	m35.asc \
	moss.asc \
	pic.asc \
	prim.asc \
	sphflake.asc \
	star.asc \
	tank_car.asc \
	terra.asc \
	truck.asc \
	wave.asc \
	woodsman.asc \
	world.asc \
	xmp.asc

EXTRA_DIST = \
	${MODELS} \
	db.php

EXTRA_DATA = ${MODELS:asc=g}
BUILT_SOURCES = ${MODELS:asc=g}

MOSTLYCLEANFILES = ${MODELS:asc=g}

MAINTAINERCLEANFILES = Makefile.in
