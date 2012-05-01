set(pl-dm_TCLSCRIPTS
  bindings.tcl
  help.tcl
  menu.tcl
  )
BRLCAD_ADDDATA(pl-dm_TCLSCRIPTS tclscripts/pl-dm)
pkgIndex_BUILD(pl-dm tclscripts/pl-dm)
tclIndex_BUILD(pl-dm tclscripts/pl-dm)

CMAKEFILES(pkgIndex.tcl tclIndex)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
