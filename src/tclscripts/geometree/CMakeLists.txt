SET(geometree_TCLSCRIPTS
	GeometryBrowser.tcl
	geometree.tcl
)
tclscripts_BUILD(geometree_TCLSCRIPTS tclscripts/geometree)
pkgIndex_BUILD(geometree tclscripts/geometree)
tclIndex_BUILD(geometree tclscripts/geometree)

