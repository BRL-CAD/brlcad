set(igraph_TCLSCRIPTS
  GraphEditor.tcl
  igraph.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(igraph_TCLSCRIPTS tclscripts/igraph)
  pkgIndex_BUILD(tclscripts/igraph)
  tclIndex_BUILD(tclscripts/igraph)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${igraph_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
