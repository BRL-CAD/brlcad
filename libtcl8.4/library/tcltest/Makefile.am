# $Id$


FILES = pkgIndex.tcl tcltest.tcl

install:
	mkdir -p @TCL_LIBRARY@/tcltest
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TCL_LIBRARY@/tcltest/$$a ; done
