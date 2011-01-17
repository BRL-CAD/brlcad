
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
	tests \
	win

DISTCLEANFILES = pkgIndex.tcl

include $(top_srcdir)/misc/Makefile.defs

