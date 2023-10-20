add_subdirectory(scripts)

set(sdialogs_TCLSCRIPTS
  sdialogs.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(sdialogs_TCLSCRIPTS tclscripts/sdialogs)
  pkgIndex_BUILD(tclscripts/sdialogs)
  tclIndex_BUILD(tclscripts/sdialogs)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${sdialogs_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
