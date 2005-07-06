# $Id$

this_dir = tclscripts/lib
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripties = \
	CellPlot.tcl \
	ColorEntry.tcl \
	ComboBox.tcl \
	Command.tcl \
	Database.tcl \
	Db.tcl \
	Display.tcl \
	Dm.tcl \
	Drawable.tcl \
	Help.tcl \
	Legend.tcl \
	Mged.tcl \
	ModelAxesControl.tcl \
	QuadDisplay.tcl \
	RtControl.tcl \
	Splash.tcl \
	Table.tcl \
	TableView.tcl \
	View.tcl \
	ViewAxesControl.tcl

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
