# $Id$

SUBDIRS = generic

INSTALL_DIR = ${libdir}/iwidgets4.0.1/

FILES = iwidgets.tcl pkgIndex.tcl

install:
	mkdir -p ${INSTALL_DIR}
	ln -fs ${INSTALL_DIR} ${libdir}/iwidgets
	for a in $(SUBDIRS) ; do (cd $$a && make install) ; done
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a ${INSTALL_DIR}/$$a ; done

