# $Id$


FILES = http.tcl pkgIndex.tcl

install:
	mkdir -p @TCL_LIBRARY@/http1.0
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TCL_LIBRARY@/http1.0/$$a ; done
