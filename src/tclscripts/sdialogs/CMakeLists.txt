add_subdirectory(scripts)

set(sdialogs_TCLSCRIPTS
  sdialogs.tcl
  )
BRLCAD_ADDDATA(sdialogs_TCLSCRIPTS tclscripts/sdialogs)
pkgIndex_BUILD(sdialogs tclscripts/sdialogs)
tclIndex_BUILD(sdialogs tclscripts/sdialogs)

CMAKEFILES(Makefile.am pkgIndex.tcl tclIndex)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
