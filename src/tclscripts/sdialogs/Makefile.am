
SUBDIRS = \
	scripts

this_dir = tclscripts/sdialogs
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripts = \
	sdialogs.tcl

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
