set(igraph_TCLSCRIPTS
  GraphEditor.tcl
  igraph.tcl
  )
BRLCAD_ADDDATA(igraph_TCLSCRIPTS tclscripts/igraph)
pkgIndex_BUILD(igraph tclscripts/igraph)
tclIndex_BUILD(igraph tclscripts/igraph)

CMAKEFILES(pkgIndex.tcl tclIndex)
CMAKEFILES(Makefile.am)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
