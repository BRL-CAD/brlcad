SET(pl-dm_TCLSCRIPTS
	bindings.tcl
	help.tcl
	menu.tcl
	)
BRLCAD_ADDDATA(pl-dm_TCLSCRIPTS tclscripts/pl-dm)
pkgIndex_BUILD(pl-dm tclscripts/pl-dm)
tclIndex_BUILD(pl-dm tclscripts/pl-dm)

CMAKEFILES(pkgIndex.tcl tclIndex)
CMAKEFILES(Makefile.am)
