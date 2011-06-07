
SUBDIRS = \
	archer \
	boteditor \
	geometree \
	hv3 \
	lib \
	mged \
	pl-dm \
	rtwizard \
	sdialogs \
	swidgets \
	util

this_dir = tclscripts
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripties = \
	ami.tcl \
	ampi.tcl \
	cad_clrpick.tcl \
	cad_dialog.tcl \
	chkexterns.tcl \
	fs_dialog.tk \
	helpcomm.tcl \
	helplib.tcl \
	hoc.tcl \
	html_library.tcl \
	libdm.tcl \
	man_browser.tcl \
	menu_override.tcl \
	mouse.tcl \
	vmath.tcl

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripties)

EXTRA_DIST = $(tcl_SCRIPTS) CMakeLists.txt

# CLEANFILES = $(BUILT_SOURCES)

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
