set(checker_TCLSCRIPTS
  check.tcl
  menu.tcl
  OverlapFileTool.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(checker_TCLSCRIPTS tclscripts/checker)
  pkgIndex_BUILD(tclscripts/checker)
  tclIndex_BUILD(tclscripts/checker)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  check.sh
  test_checker.tcl
  TODO
  ${checker_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
