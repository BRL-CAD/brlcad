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
	BotUtility.tcl
	CombEditFrame.tcl
	DataUtils.tcl
	EhyEditFrame.tcl
	EllEditFrame.tcl
	EpaEditFrame.tcl
	EtoEditFrame.tcl
	ExtrudeEditFrame.tcl
	GeometryEditFrame.tcl
	GripEditFrame.tcl
	HalfEditFrame.tcl
	HypEditFrame.tcl
	LoadArcherLibs.tcl
	PartEditFrame.tcl
	PipeEditFrame.tcl
	Plugin.tcl
	RhcEditFrame.tcl
	RpcEditFrame.tcl
	ShaderEdit.tcl
	SphereEditFrame.tcl
	SuperellEditFrame.tcl
	TgcEditFrame.tcl
	TorusEditFrame.tcl
	Utility.tcl
	Wizard.tcl
	bgerror.tcl
	itk_redefines.tcl
	tabwindow.itk
)
BRLCAD_ADDDATA(archer_TCLSCRIPTS tclscripts/archer)
pkgIndex_BUILD(archer tclscripts/archer)
tclIndex_BUILD(archer tclscripts/archer)

CMAKEFILES(pkgIndex.tcl tclIndex)
CMAKEFILES(Makefile.am)
