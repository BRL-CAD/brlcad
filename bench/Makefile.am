# $Id$

bin_PROGRAMS = pixcmp
pixcmp_SOURCES = pixcmp.c

clobber:
	@echo "About to wipe out all pictures and binary files in this directory"
	@echo "Send SIGINT (typ. ^C) within 3 seconds to abort"
	@sleep 3
	rm -f *.log.* *.pix.*	# old versions
	rm -f *.log *.pix	# last try

EXTRA_DIST = lgt.sh perf.sh pretty.sh recheck.sh run.sh try.sh

benchmark: all
	sh run.sh

bench: benchmark
