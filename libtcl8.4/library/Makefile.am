# $Id$

SUBDIRS = dde http encoding http1.0 msgcat opt reg tcltest

FILES = auto.tcl history.tcl init.tcl ldAout.tcl package.tcl parray.tcl safe.tcl tclIndex word.tcl

install:
	mkdir -p @TCL_LIBRARY@
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TCL_LIBRARY@/$$a ; done
	for a in $(SUBDIRS) ; do (cd $$a && make install) ; done
