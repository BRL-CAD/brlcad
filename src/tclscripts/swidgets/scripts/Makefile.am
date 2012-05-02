
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

EXTRA_DIST = $(tcl_SCRIPTS) CMakeLists.txt

# CLEANFILES = $(BUILT_SOURCES)

tclIndex:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
