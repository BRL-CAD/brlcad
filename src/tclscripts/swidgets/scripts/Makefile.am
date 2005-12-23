
this_dir = tclscripts/swidgets/scripts
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripts = \
	selectlists.itk \
	tkgetdir.itk \
	togglearrow.itk \
	tooltip.itk \
	tree.itk

BUILT_SOURCES = \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripts)

EXTRA_DIST = $(scripts)

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
