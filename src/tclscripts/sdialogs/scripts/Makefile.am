
this_dir = tclscripts/sdialogs/scripts
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripts = \
	entrydialog.itk \
	listdialog.itk \
	stddlgs.tcl

BUILT_SOURCES = \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripts)

EXTRA_DIST = $(scripts)

# CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
