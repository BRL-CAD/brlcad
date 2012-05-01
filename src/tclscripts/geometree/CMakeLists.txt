set(geometree_TCLSCRIPTS
  GeometryBrowser.tcl
  geometree.tcl
  )
BRLCAD_ADDDATA(geometree_TCLSCRIPTS tclscripts/geometree)
pkgIndex_BUILD(geometree tclscripts/geometree)
tclIndex_BUILD(geometree tclscripts/geometree)

CMAKEFILES(pkgIndex.tcl tclIndex)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
