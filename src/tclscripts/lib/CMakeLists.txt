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
)

FILE(COPY ${lib_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${lib_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/lib)

pkgIndex_BUILD(lib ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/lib)
tclIndex_BUILD(lib ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/lib)

