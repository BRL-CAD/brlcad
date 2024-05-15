set(geometree_TCLSCRIPTS
  GeometryBrowser.tcl
  geometree.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(geometree_TCLSCRIPTS tclscripts/geometree)
  pkgIndex_BUILD(tclscripts/geometree)
  tclIndex_BUILD(tclscripts/geometree)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${geometree_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
