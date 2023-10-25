add_subdirectory(images)
add_subdirectory(scripts)

set(swidgets_TCLSCRIPTS
  swidgets.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(swidgets_TCLSCRIPTS tclscripts/swidgets)
  pkgIndex_BUILD(tclscripts/swidgets)
  tclIndex_BUILD(tclscripts/swidgets)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${swidgets_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
