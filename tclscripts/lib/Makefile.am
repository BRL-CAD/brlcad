# $Id$

tcldir = $(prefix)/tclscripts/lib

tcl_SCRIPTS = CellPlot.tcl ColorEntry.tcl ComboBox.tcl Command.tcl Database.tcl Db.tcl Display.tcl Dm.tcl Drawable.tcl Help.tcl Legend.tcl Mged.tcl QuadDisplay.tcl RtControl.tcl Splash.tcl Table.tcl TableView.tcl View.tcl


tcl_DATA = tclIndex

.PHONY : tclIndex
tclIndex:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ami.tcl $(top_srcdir)/tclscripts/lib
