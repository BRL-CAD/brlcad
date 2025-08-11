set(util_TCLSCRIPTS expand_int_ranges.tcl luniq.tcl metastrength.tcl regionstobots.tcl)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(util_TCLSCRIPTS tclscripts/util)
  pkgindex_build(tclscripts/util)
  tclindex_build(tclscripts/util)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${util_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
