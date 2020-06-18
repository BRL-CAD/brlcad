set(tcllib_TCLSCRIPTS
  cmdline.tcl
  fileutil.tcl
  )
BRLCAD_ADDDATA(tcllib_TCLSCRIPTS tclscripts/tcllib)
pkgIndex_BUILD(tclscripts/tcllib)
tclIndex_BUILD(tclscripts/tcllib)

CMAKEFILES(license.terms)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
