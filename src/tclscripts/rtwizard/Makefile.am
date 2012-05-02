
SUBDIRS = examples lib

this_dir = tclscripts/rtwizard
tcldir = $(BRLCAD_DATA)/$(this_dir)

dist_bin_SCRIPTS = \
	rtwizard

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	RaytraceWizard.tcl \
	rtwizard.bat \
	rtwizard.tcl

EXTRA_DIST = $(tcl_SCRIPTS) CMakeLists.txt

rtwizard: rtwizard.tcl
	cp $(top_srcdir)/src/tclscripts/rtwizard/rtwizard.tcl rtwizard

CLEANFILES = rtwizard


pkgIndex.tcl:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
