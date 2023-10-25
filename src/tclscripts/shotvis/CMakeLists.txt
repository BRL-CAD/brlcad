set(shotvis_TCLSCRIPTS
  shotvis.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(shotvis_TCLSCRIPTS tclscripts/shotvis)
  pkgIndex_BUILD(tclscripts/shotvis)
  tclIndex_BUILD(tclscripts/shotvis)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${shotvis_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
