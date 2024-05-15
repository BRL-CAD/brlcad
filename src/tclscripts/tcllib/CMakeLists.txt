set(tcllib_TCLSCRIPTS
  cmdline.tcl
  fileutil.tcl
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(tcllib_TCLSCRIPTS tclscripts/tcllib)
  pkgIndex_BUILD(tclscripts/tcllib)
  tclIndex_BUILD(tclscripts/tcllib)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  license.terms
  ${tcllib_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
