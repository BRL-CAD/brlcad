# $Id$


FILES = msgcat.tcl pkgIndex.tcl

install:
	mkdir -p @TCL_LIBRARY@/msgcat
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TCL_LIBRARY@/msgcat/$$a ; done
