# $Id$

bin_PROGRAMS = pixcmp
pixcmp_SOURCES = pixcmp.c

clobber:
	@$(ECHO) "About to wipe out all pictures and binary files in this directory"
	@$(ECHO) "Send SIGINT (typ. ^C) within 3 seconds to abort"
	@sleep 3
	rm -f *.log.* *.pix.*	# old versions
	rm -f *.log *.pix	# last try

EXTRA_DIST = \
	lgt.sh \
	perf.sh \
	pretty.sh \
	recheck.sh \
	run.sh \
	try.sh

.PHONY : benchmark bench

benchmark: all
	${SH} run.sh

bench: benchmark
