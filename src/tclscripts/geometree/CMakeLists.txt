set(geometree_TCLSCRIPTS GeometryBrowser.tcl geometree.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(geometree_TCLSCRIPTS tclscripts/geometree)
  pkgindex_build(tclscripts/geometree)
  tclindex_build(tclscripts/geometree)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${geometree_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
