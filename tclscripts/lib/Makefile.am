# $Id$

tcldir = $(prefix)/tclscripts/lib

dist_tcl_SCRIPTS = CellPlot.tcl ColorEntry.tcl ComboBox.tcl Command.tcl Database.tcl Db.tcl Display.tcl Dm.tcl Drawable.tcl Help.tcl Legend.tcl Mged.tcl QuadDisplay.tcl RtControl.tcl Splash.tcl Table.tcl TableView.tcl View.tcl

install-data-hook:
	${bindir}/btclsh $(top_srcdir)/tclscripts/ami.tcl $(PWD) && ${INSTALL_DATA} tclIndex $(tcldir)/tclIndex

