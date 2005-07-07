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

BENCHMARK_MODELS = \
	bldg391.asc \
	m35.asc \
	moss.asc \
	sphflake.asc \
	star.asc \
	world.asc

MODELS = \
	$(BENCHMARK_MODELS) \
	axis.asc \
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
	pic.asc \
	prim.asc \
	tank_car.asc \
	terra.asc \
	truck.asc \
	wave.asc \
	woodsman.asc \
	xmp.asc

EXTRA_DIST = \
	${MODELS} \
	db.php

dbdir = $(BRLCAD_DATA)/db

if INSTALL_MODELS
db_DATA = ${MODELS:asc=g}
BUILT_SOURCES = ${MODELS:asc=g}

else !INSTALL_MODELS
db_DATA = ${BENCHMARK_MODELS:asc=g}
BUILT_SOURCES = ${BENCHMARK_MODELS:asc=g}

endif
# end INSTALL_MODELS


MOSTLYCLEANFILES = ${MODELS:asc=g}

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

