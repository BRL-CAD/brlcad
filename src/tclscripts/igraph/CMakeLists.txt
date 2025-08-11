set(igraph_TCLSCRIPTS GraphEditor.tcl igraph.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(igraph_TCLSCRIPTS tclscripts/igraph)
  pkgindex_build(tclscripts/igraph)
  tclindex_build(tclscripts/igraph)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${igraph_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
