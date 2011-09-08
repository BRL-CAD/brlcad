
.PHONY : clean clobber run bench

bin_PROGRAMS = pixcmp
pixcmp_SOURCES = pixcmp.c
pixcmp_CFLAGS = ${TCL_CPPFLAGS}
pixcmp_LDADD = ${BU} ${BU_LIBS}

dist_bin_SCRIPTS = \
	benchmark

man_MANS = \
	benchmark.1 \
	pixcmp.1

EXTRA_DIST = \
	$(man_MANS) \
	${dist_bin_SCRIPTS} \
	lgt.sh \
	run.sh \
	try.sh \
	viewdiff.sh \
	CMakeLists.txt

benchmark: run.sh
	cp $(top_srcdir)/bench/run.sh benchmark

run: all
	@if test -r ${srcdir}/run.sh ; then \
		LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
		DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
		${SH} ${srcdir}/run.sh run ; \
	        ret=$$? ; \
		if test "x$$ret" != "x0" ; then \
			echo "*** BENCHMARK TESTING FAILED ***" ; \
			exit 1 ; \
		fi ; \
	fi
	@echo "---"
	@echo "Run 'make clean' in the bench directory to delete the log"
	@echo "and image files generated during the benchmark analysis."
	@echo

bench: run

clean-local:
	@${SH} ${srcdir}/run.sh clean

distclean-local:
	@${SH} ${srcdir}/run.sh clean

clobber:
	@${SH} ${srcdir}/run.sh clobber

CLEANFILES = \
	*.log \
	*.pix \
	benchmark \
	pixcmp

DEPS=src/rt db

include $(top_srcdir)/misc/Makefile.defs
