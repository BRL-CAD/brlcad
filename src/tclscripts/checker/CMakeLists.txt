set(checker_TCLSCRIPTS
  check.tcl
  menu.tcl
  OverlapFileTool.tcl
  )
BRLCAD_ADDDATA(checker_TCLSCRIPTS tclscripts/checker)
pkgIndex_BUILD(tclscripts/checker)
tclIndex_BUILD(tclscripts/checker)

CMAKEFILES(check.sh test_checker.tcl TODO)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
