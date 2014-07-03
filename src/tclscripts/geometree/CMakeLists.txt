set(geometree_TCLSCRIPTS
  GeometryBrowser.tcl
  geometree.tcl
  )
BRLCAD_ADDDATA(geometree_TCLSCRIPTS tclscripts/geometree)
pkgIndex_BUILD(tclscripts/geometree)
tclIndex_BUILD(tclscripts/geometree)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
