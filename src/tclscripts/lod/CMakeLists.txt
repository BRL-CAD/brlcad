set(lod_TCLSCRIPTS LODDialog.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(lod_TCLSCRIPTS tclscripts/lod)
  tclindex_build(tclscripts/lod)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${lod_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
