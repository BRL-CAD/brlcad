set(plot3-dm_TCLSCRIPTS bindings.tcl help.tcl menu.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(plot3-dm_TCLSCRIPTS tclscripts/plot3-dm)
  pkgindex_build(tclscripts/plot3-dm)
  tclindex_build(tclscripts/plot3-dm)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${plot3-dm_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
