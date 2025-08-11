set(sdialogs_scripts_TCLSCRIPTS entrydialog.itk listdialog.itk stddlgs.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(sdialogs_scripts_TCLSCRIPTS tclscripts/sdialogs/scripts)
  tclindex_build(tclscripts/sdialogs/scripts)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${sdialogs_scripts_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
