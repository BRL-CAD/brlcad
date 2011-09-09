SET(lib_TCLSCRIPTS
	CellPlot.tcl
	ColorEntry.tcl
	ComboBox.tcl
	Command.tcl
	Database.tcl
	Db.tcl
	Display.tcl
	Dm.tcl
	Drawable.tcl
	Ged.tcl
	Help.tcl
	Legend.tcl
	Mged.tcl
	ModelAxesControl.tcl
	QuadDisplay.tcl
	RtControl.tcl
	Splash.tcl
	Table.tcl
	TableView.tcl
	TkTable.tcl
	View.tcl
	ViewAxesControl.tcl
	cursor.tcl
)
BRLCAD_ADDDATA(lib_TCLSCRIPTS tclscripts/lib)
pkgIndex_BUILD(lib tclscripts/lib)
tclIndex_BUILD(lib tclscripts/lib)
