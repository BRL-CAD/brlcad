# $Id$

SUBDIRS = generic doc

INSTALL_DIR = ${libdir}/iwidgets4.0.1

brlcadscriptdir = ${INSTALL_DIR}
brlcadscript_SCRIPTS = iwidgets.tcl pkgIndex.tcl license.terms

EXTRA_DIST = \
	$(brlcadscript_SCRIPTS) \
	CHANGES \
	aclocal.m4 \
	demos \
	incoming \
	outgoing \
	tcl.m4 \
	tests \
	win

DISTCLEANFILES = \
	iwidgets.tcl \
	pkgIndex.tcl

MAINTAINERCLEANFILES = Makefile.in

install-script-hook:
	ln -fs ${INSTALL_DIR} ${libdir}/iwidgets
