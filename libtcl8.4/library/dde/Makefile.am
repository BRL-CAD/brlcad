# $Id$

FILES = pkgIndex.tcl

install:
	mkdir -p @TCL_LIBRARY@/dde
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TCL_LIBRARY@/dde/$$a ; done
