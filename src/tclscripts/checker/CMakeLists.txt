set(checker_TCLSCRIPTS
  check.tcl
  )
BRLCAD_ADDDATA(checker_TCLSCRIPTS tclscripts/checker)
pkgIndex_BUILD(tclscripts/checker)
tclIndex_BUILD(tclscripts/checker)

CMAKEFILES(check.sh)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
