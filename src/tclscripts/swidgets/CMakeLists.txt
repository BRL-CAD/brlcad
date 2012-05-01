add_subdirectory(images)
add_subdirectory(scripts)

set(swidgets_TCLSCRIPTS
  swidgets.tcl
  )
BRLCAD_ADDDATA(swidgets_TCLSCRIPTS tclscripts/swidgets)
pkgIndex_BUILD(swidgets tclscripts/swidgets)
tclIndex_BUILD(swidgets tclscripts/swidgets)

CMAKEFILES(pkgIndex.tcl tclIndex)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
