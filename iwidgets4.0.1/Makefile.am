# $Id$

SUBDIRS = generic doc

INSTALL_DIR = ${libdir}/iwidgets4.0.1

brlcadscriptdir = ${INSTALL_DIR}
brlcadscript_SCRIPTS = iwidgets.tcl pkgIndex.tcl license.terms
EXTRA_DIST = $(brlcadscript_SCRIPTS) 

install-script-hook:
	ln -fs ${INSTALL_DIR} ${libdir}/iwidgets

