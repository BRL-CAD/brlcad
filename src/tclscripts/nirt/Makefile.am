# $Id$

this_dir = tclscripts/nirt
tcldir = $(prefix)/$(this_dir)

dist_tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	prototype.sh

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

EXTRA_DIST = $(dist_tcl_SCRIPTS)

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
