set(util_TCLSCRIPTS
  expand_int_ranges.tcl
  luniq.tcl
  metastrength.tcl
  regionstobots.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(util_TCLSCRIPTS tclscripts/util)
  pkgIndex_BUILD(tclscripts/util)
  tclIndex_BUILD(tclscripts/util)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${util_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
