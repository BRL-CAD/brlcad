
this_dir = tclscripts/lib
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripties = \
	CellPlot.tcl \
	ColorEntry.tcl \
	ComboBox.tcl \
	Command.tcl \
	cursor.tcl \
	Database.tcl \
	Db.tcl \
	Display.tcl \
	Dm.tcl \
	Drawable.tcl \
	Ged.tcl \
	Help.tcl \
	Legend.tcl \
	Mged.tcl \
	ModelAxesControl.tcl \
	QuadDisplay.tcl \
	RtControl.tcl \
	Splash.tcl \
	Table.tcl \
	TableView.tcl \
	TkTable.tcl \
	View.tcl \
	ViewAxesControl.tcl

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
