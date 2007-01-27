# $Id$

bin_PROGRAMS = pixcmp
pixcmp_SOURCES = pixcmp.c

clobber:
	@$(ECHO) "About to wipe out all pictures and binary files in this directory"
	@$(ECHO) "Send SIGINT (type ^C) within 3 seconds to abort"
	@sleep 3
	rm -f *.log.* *.pix.*	# old versions
	rm -f *.log *.pix	# last try

dist_bin_SCRIPTS = \
	benchmark

man_MANS = \
	benchmark.1

EXTRA_DIST = \
	$(man_MANS) \
	${dist_bin_SCRIPTS} \
	lgt.sh \
	run.sh \
	try.sh \
	viewdiff.sh

benchmark: run.sh
	cp $(top_srcdir)/bench/run.sh benchmark

.PHONY : run bench

run: all
	${SH} run.sh

bench: run

CLEANFILES = \
	benchmark

include $(top_srcdir)/misc/Makefile.defs
