add_subdirectory(images)
add_subdirectory(scripts)

set(swidgets_TCLSCRIPTS
  swidgets.tcl
  )
BRLCAD_ADDDATA(swidgets_TCLSCRIPTS tclscripts/swidgets)
pkgIndex_BUILD(swidgets tclscripts/swidgets)
tclIndex_BUILD(swidgets tclscripts/swidgets)

CMAKEFILES(Makefile.am pkgIndex.tcl tclIndex)
