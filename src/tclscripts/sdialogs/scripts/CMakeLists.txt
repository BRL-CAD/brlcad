set(sdialogs_scripts_TCLSCRIPTS
  entrydialog.itk
  listdialog.itk
  stddlgs.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(sdialogs_scripts_TCLSCRIPTS tclscripts/sdialogs/scripts)
  tclIndex_BUILD(tclscripts/sdialogs/scripts)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${sdialogs_scripts_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
