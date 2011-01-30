SET(util_TCLSCRIPTS
	expand_int_ranges.tcl
	luniq.tcl
	)
tclscripts_BUILD(util_TCLSCRIPTS tclscripts/util)
pkgIndex_BUILD(util tclscripts/util)
tclIndex_BUILD(util tclscripts/util)

