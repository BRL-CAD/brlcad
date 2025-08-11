add_subdirectory(scripts)

set(sdialogs_TCLSCRIPTS sdialogs.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(sdialogs_TCLSCRIPTS tclscripts/sdialogs)
  pkgindex_build(tclscripts/sdialogs)
  tclindex_build(tclscripts/sdialogs)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${sdialogs_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
