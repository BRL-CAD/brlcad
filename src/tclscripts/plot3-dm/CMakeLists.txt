set(plot3-dm_TCLSCRIPTS
  bindings.tcl
  help.tcl
  menu.tcl
  )
BRLCAD_ADDDATA(plot3-dm_TCLSCRIPTS tclscripts/plot3-dm)
pkgIndex_BUILD(tclscripts/plot3-dm)
tclIndex_BUILD(tclscripts/plot3-dm)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
