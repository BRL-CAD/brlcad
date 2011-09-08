
SUBDIRS = include

A2G=$(top_builddir)/src/conv/asc2g
G2A=$(top_builddir)/src/conv/g2asc


# gmake seems to prefix the source .asc with the appropriate srcdir
# automagically...  the library path is set for Tcl/Tk since libtool
# won't do it for us for non libtool libs.  make them all depend on a
# single file so that asc2g runs at least once to completion so the
# wrapper script doesn't fail when compiling in parallel.
.asc.g: operators.asc2g
	@i=0 ; while [ ! -f lockfile ] && [ $$i -lt 10 ] ; do sleep 1 ; i=`expr $$i + 1` ; done
	@echo $(A2G) $< $@ ; \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(A2G) `if test -e $< ; then echo $< ; else echo $(top_srcdir)/db/$< ; fi` $@

# cause asc2g to kick off so that it compiles lt-asc2g and avoids
# parallel build failures.  use a simple file locking mechanism.
operators.asc2g: operators.asc $(top_builddir)/src/conv/asc2g
	@rm -f lockfile
	@if test -x $(A2G) ; then \
		echo $(A2G) operators.asc $@ ; \
		LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
		DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
		$(A2G) `if test -e operators.asc ; then echo operators.asc ; else echo $(top_srcdir)/db/operators.asc ; fi` $@ ; \
	else \
		echo "ls -lad $(A2G)" ; \
		echo "`ls -lad $(A2G)`" ; \
		echo "$(A2G) does not exist" ; \
	fi
	@touch lockfile

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
	cornell-kunigami.asc \
	cray.asc \
	crod.asc \
	cube.asc \
	demo.asc \
	galileo.asc \
	goliath.asc \
	havoc.asc \
	kman.asc \
	ktank.asc \
	lgt-test.asc \
	operators.asc \
	pic.asc \
	pinewood.asc \
	prim.asc \
	tank_car.asc \
	terra.asc \
	toyjeep.asc \
	truck.asc \
	wave.asc \
	woodsman.asc \
	xmp.asc

EXTRA_DIST = \
	${MODELS} \
	cornell.rt \
	db.php \
	terra.dsp \
	CMakeLists.txt

dbdir = $(BRLCAD_DATA)/db

if INSTALL_MODELS
BUILT_SOURCES = \
	operators.asc2g \
	${MODELS:asc=g}
db_DATA = \
	$(BUILT_SOURCES) \
	terra.dsp

else !INSTALL_MODELS
BUILT_SOURCES = \
	operators.asc2g \
	${BENCHMARK_MODELS:asc=g}
db_DATA = $(BUILT_SOURCES)

endif
# end INSTALL_MODELS


MOSTLYCLEANFILES = \
	${BUILT_SOURCES} \
	lockfile

DEPS=src/conv

include $(top_srcdir)/misc/Makefile.defs
