set(lib_TCLSCRIPTS
	Accordian.tcl
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
	GeometryLoad.tcl
	Help.tcl
	Legend.tcl
	Mged.tcl
	ModelAxesControl.tcl
	QuadDisplay.tcl
	RtControl.tcl
	RtImage.tcl
	Splash.tcl
	Table.tcl
	TableView.tcl
	TkTable.tcl
	View.tcl
	ViewAxesControl.tcl
	cursor.tcl
	apply_mat.tcl
	pattern.tcl
	pattern_gui.tcl
)
BRLCAD_ADDDATA(lib_TCLSCRIPTS tclscripts/lib)
pkgIndex_BUILD(tclscripts/lib)
tclIndex_BUILD(tclscripts/lib)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
