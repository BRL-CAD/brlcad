# $Id$


FILES = optparse.tcl pkgIndex.tcl

install:
	mkdir -p @TCL_LIBRARY@/opt
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TCL_LIBRARY@/opt/$$a ; done
