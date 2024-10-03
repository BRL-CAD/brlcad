set(tcllib_TCLSCRIPTS cmdline.tcl fileutil.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(tcllib_TCLSCRIPTS tclscripts/tcllib)
  pkgindex_build(tclscripts/tcllib)
  tclindex_build(tclscripts/tcllib)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
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
