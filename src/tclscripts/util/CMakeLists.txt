set(util_TCLSCRIPTS
  expand_int_ranges.tcl
  getopt.tcl
  luniq.tcl
  )
BRLCAD_ADDDATA(util_TCLSCRIPTS tclscripts/util)
pkgIndex_BUILD(tclscripts/util)
tclIndex_BUILD(tclscripts/util)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
