set(checker_TCLSCRIPTS check.tcl menu.tcl OverlapFileTool.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(checker_TCLSCRIPTS tclscripts/checker)
  pkgindex_build(tclscripts/checker)
  tclindex_build(tclscripts/checker)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
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
