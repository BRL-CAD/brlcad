# $Id$

SUBDIRS = \
	doc \
	generic \
	library

AM_CFLAGS = ${X_CFLAGS}

brlcadscriptdir = $(libdir)/itk@ITCL_VERSION@
brlcadscript_SCRIPTS = pkgIndex.tcl

EXTRA_DIST = \
	$(brlcadscript_SCRIPTS) \
	aclocal.m4 \
	demos \
	examples \
	itkConfig.sh.in \
	license.terms \
	mac \
	tests \
	win

DISTCLEANFILES = pkgIndex.tcl

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

