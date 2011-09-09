SET(geometree_TCLSCRIPTS
	GeometryBrowser.tcl
	geometree.tcl
)
BRLCAD_ADDDATA(geometree_TCLSCRIPTS tclscripts/geometree)
pkgIndex_BUILD(geometree tclscripts/geometree)
tclIndex_BUILD(geometree tclscripts/geometree)
