# $Id$

SUBDIRS = \
	generic \
	library

lib_LTLIBRARIES = \
	libitk.la \
	libitk@ITCL_VERSION@.la

libitk_la_LIBADD = generic/libitg.la
libitk_la_SOURCES =
libitk@ITCL_VERSION@_la_LIBADD = $(libitk_la_LIBADD)
libitk@ITCL_VERSION@_la_SOURCES = $(libitk_la_SOURCES)

AM_CFLAGS = ${X_CFLAGS}

brlcadscriptdir = $(libdir)/itk@ITCL_VERSION@
brlcadscript_SCRIPTS = pkgIndex.tcl

EXTRA_DIST = \
	$(brlcadscript_SCRIPTS) \
	aclocal.m4 \
	demos \
	doc \
	examples \
	itkConfig.sh.in \
	license.terms \
	mac \
	tests \
	win

DISTCLEANFILES = pkgIndex.tcl

MAINTAINERCLEANFILES = Makefile.in
