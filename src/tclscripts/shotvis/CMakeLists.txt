set(shotvis_TCLSCRIPTS
  shotvis.tcl
  )
BRLCAD_ADDDATA(shotvis_TCLSCRIPTS tclscripts/shotvis)
pkgIndex_BUILD(tclscripts/shotvis)
tclIndex_BUILD(tclscripts/shotvis)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
