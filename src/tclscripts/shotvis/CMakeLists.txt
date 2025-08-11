set(shotvis_TCLSCRIPTS shotvis.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(shotvis_TCLSCRIPTS tclscripts/shotvis)
  pkgindex_build(tclscripts/shotvis)
  tclindex_build(tclscripts/shotvis)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${shotvis_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
