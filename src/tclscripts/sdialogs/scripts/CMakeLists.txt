SET(sdialogs_scripts_TCLSCRIPTS
	entrydialog.itk
	listdialog.itk
	stddlgs.tcl
)
tclscripts_BUILD(sdialogs_scripts_TCLSCRIPTS tclscripts/sdialogs/scripts)
tclIndex_BUILD(sdialogs_scripts tclscripts/sdialogs/scripts)

