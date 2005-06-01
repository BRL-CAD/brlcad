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
	benchmark \
	perf.sh

EXTRA_DIST = \
	${dist_bin_SCRIPTS} \
	lgt.sh \
	pretty.sh \
	recheck.sh \
	run.sh \
	try.sh

benchmark: run.sh
	cp run.sh benchmark

.PHONY : run bench

run: all
	${SH} run.sh

bench: run

CLEANFILES = \
	benchmark
