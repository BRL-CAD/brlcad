# $Id$

SUBDIRS = generic

INSTALL_DIR = ${libdir}/iwidgets4.0.1

brlcadscriptdir = ${INSTALL_DIR}
brlcadscript_SCRIPTS = iwidgets.tcl pkgIndex.tcl

install-script-hook:
	ln -fs ${INSTALL_DIR} ${libdir}/iwidgets

