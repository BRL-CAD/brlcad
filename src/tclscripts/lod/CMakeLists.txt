set(lod_TCLSCRIPTS
  LODDialog.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(lod_TCLSCRIPTS tclscripts/lod)
  tclIndex_BUILD(tclscripts/lod)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${lod_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
