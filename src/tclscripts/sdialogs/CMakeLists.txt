ADD_SUBDIRECTORY(scripts)

set(sdialogs_TCLSCRIPTS
	sdialogs.tcl
)
BRLCAD_ADDDATA(sdialogs_TCLSCRIPTS tclscripts/sdialogs)
pkgIndex_BUILD(sdialogs tclscripts/sdialogs)
tclIndex_BUILD(sdialogs tclscripts/sdialogs)

CMAKEFILES(Makefile.am pkgIndex.tcl tclIndex)
