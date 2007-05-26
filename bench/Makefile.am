# $Id$

bin_PROGRAMS = pixcmp
pixcmp_SOURCES = pixcmp.c

clobber:
	@${SH} run.sh clobber

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
	viewdiff.sh

benchmark: run.sh
	cp $(top_srcdir)/bench/run.sh benchmark

.PHONY : clobber run bench

run: all
	@${SH} run.sh
	@$(ECHO) "---"
	@$(ECHO) "Run 'make clobber' to delete the log and image files generated"
	@$(ECHO) "during the benchmark analysis."
	@$(ECHO)

bench: run

CLEANFILES = \
	benchmark

include $(top_srcdir)/misc/Makefile.defs
