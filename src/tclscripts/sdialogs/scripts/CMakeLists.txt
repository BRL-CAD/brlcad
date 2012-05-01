set(sdialogs_scripts_TCLSCRIPTS
	entrydialog.itk
	listdialog.itk
	stddlgs.tcl
)
BRLCAD_ADDDATA(sdialogs_scripts_TCLSCRIPTS tclscripts/sdialogs/scripts)
tclIndex_BUILD(sdialogs_scripts tclscripts/sdialogs/scripts)

CMAKEFILES(tclIndex)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
