# $Id$
SUBDIRS = examples lib

this_dir = tclscripts/rtwizard
tcldir = $(prefix)/$(this_dir)

scripties = rtwizard.tcl

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripties)

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

EXTRA_DIST = $(scripties)

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
