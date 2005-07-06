# $Id$

this_dir = tclscripts/util
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripties = \
	expand_int_ranges.tcl \
	luniq.tcl

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripties)

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

EXTRA_DIST = $(scripties)

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
