
SUBDIRS = \
	images \
	scripts

this_dir = tclscripts/swidgets
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripts = \
	swidgets.tcl

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripts)

EXTRA_DIST = $(scripts)

# CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
