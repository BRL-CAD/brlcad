# $Id$


FILES = pkgIndex.tcl

install:
	mkdir -p @TCL_LIBRARY@/reg
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TCL_LIBRARY@/reg/$$a ; done
