ADD_SUBDIRECTORY(images)

SET(archer_TCLSCRIPTS
	Arb4EditFrame.tcl 
	Arb5EditFrame.tcl 
	Arb6EditFrame.tcl 
	Arb7EditFrame.tcl 
	Arb8EditFrame.tcl 
	Archer.tcl 
	ArcherCore.tcl 
	AttrGroupsDisplayUtility.tcl 
	bgerror.tcl 
	BotUtility.tcl 
	CombEditFrame.tcl 
	cursor.tcl 
	EhyEditFrame.tcl 
	EllEditFrame.tcl 
	EpaEditFrame.tcl 
	EtoEditFrame.tcl 
	ExtrudeEditFrame.tcl 
	GeometryEditFrame.tcl 
	GripEditFrame.tcl 
	HalfEditFrame.tcl 
	HypEditFrame.tcl 
	itk_redefines.tcl
	LoadArcherLibs.tcl 
	PartEditFrame.tcl 
	Plugin.tcl 
	RhcEditFrame.tcl 
	RpcEditFrame.tcl 
	ShaderEdit.tcl 
	SphereEditFrame.tcl 
	SuperellEditFrame.tcl 
	tabwindow.itk 
	TgcEditFrame.tcl 
	TorusEditFrame.tcl 
	Utility.tcl 
	Wizard.tcl
)
FILE(COPY ${archer_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/archer)
INSTALL(FILES ${archer_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/archer)

pkgIndex_BUILD(archer tclscripts/archer)
tclIndex_BUILD(archer tclscripts/archer)

