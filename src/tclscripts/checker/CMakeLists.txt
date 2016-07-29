set(checker_TCLSCRIPTS
  GeometryChecker.tcl
  check.tcl
  )
BRLCAD_ADDDATA(checker_TCLSCRIPTS tclscripts/checker)
pkgIndex_BUILD(tclscripts/checker)
tclIndex_BUILD(tclscripts/checker)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
