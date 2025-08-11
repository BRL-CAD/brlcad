add_subdirectory(images)
add_subdirectory(scripts)

set(swidgets_TCLSCRIPTS swidgets.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(swidgets_TCLSCRIPTS tclscripts/swidgets)
  pkgindex_build(tclscripts/swidgets)
  tclindex_build(tclscripts/swidgets)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${swidgets_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
