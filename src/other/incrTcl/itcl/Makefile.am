# $Id$

SUBDIRS = \
	generic \
	library

lib_LTLIBRARIES = \
	libitcl.la \
	libitcl@ITCL_VERSION@.la

libitcl_la_LIBADD = generic/libitg.la
libitcl_la_SOURCES =
libitcl@ITCL_VERSION@_la_LIBADD = $(libitcl_la_LIBADD)
libitcl@ITCL_VERSION@_la_SOURCES = $(libitcl_la_SOURCES)

brlcadscriptdir = $(libdir)/itcl@ITCL_VERSION@
brlcadscript_SCRIPTS = pkgIndex.tcl

EXTRA_DIST = \
	$(brlcadscript_SCRIPTS) \
	aclocal.m4 \
	doc \
	itclConfig.sh.in \
	license.terms \
	mac \
	tests \
	win

DISTCLEANFILES = pkgIndex.tcl

MAINTAINERCLEANFILES = Makefile.in

