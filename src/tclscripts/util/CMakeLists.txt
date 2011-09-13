SET(util_TCLSCRIPTS
	expand_int_ranges.tcl
	luniq.tcl
	)
BRLCAD_ADDDATA(util_TCLSCRIPTS tclscripts/util)
pkgIndex_BUILD(util tclscripts/util)
tclIndex_BUILD(util tclscripts/util)

CMAKEFILES(Makefile.am pkgIndex.tcl tclIndex)
