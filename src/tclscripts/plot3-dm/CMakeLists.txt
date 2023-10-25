set(plot3-dm_TCLSCRIPTS
  bindings.tcl
  help.tcl
  menu.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(plot3-dm_TCLSCRIPTS tclscripts/plot3-dm)
  pkgIndex_BUILD(tclscripts/plot3-dm)
  tclIndex_BUILD(tclscripts/plot3-dm)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${plot3-dm_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
