ADD_SUBDIRECTORY(scripts)

SET(sdialogs_TCLSCRIPTS
	sdialogs.tcl
)
tclscripts_BUILD(sdialogs_TCLSCRIPTS tclscripts/sdialogs)
pkgIndex_BUILD(sdialogs tclscripts/sdialogs)
tclIndex_BUILD(sdialogs tclscripts/sdialogs)

