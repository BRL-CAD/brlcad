# $Id$

SUBDIRS = generic

INSTALL_DIR = ${libdir}/iwidgets4.0.1/

install:
	mkdir -p ${INSTALL_DIR}
	for a in $(SUBDIRS) ; do (cd $$a && make install) ; done
	ln -s ${INSTALLDIR} ${libdir}/iwidgets



