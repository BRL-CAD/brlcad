# $Id$

this_dir = tclscripts/lib
tcldir = $(prefix)/$(this_dir)

dist_tcl_SCRIPTS = \
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

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

EXTRA_DIST = $(dist_tcl_SCRIPTS)

DISTCLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
