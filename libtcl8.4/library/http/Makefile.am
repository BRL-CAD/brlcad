# $Id$


FILES = http.tcl pkgIndex.tcl

install:
	mkdir -p @TCL_LIBRARY@/http
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TCL_LIBRARY@/http/$$a ; done
