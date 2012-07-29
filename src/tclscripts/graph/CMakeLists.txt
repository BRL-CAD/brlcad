set(graph_TCLSCRIPTS
  GraphEditor.tcl
  graph.tcl
  )
BRLCAD_ADDDATA(graph_TCLSCRIPTS tclscripts/graph)
pkgIndex_BUILD(graph tclscripts/graph)
tclIndex_BUILD(graph tclscripts/graph)

CMAKEFILES(pkgIndex.tcl tclIndex)
CMAKEFILES(Makefile.am)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
