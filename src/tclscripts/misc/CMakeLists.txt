set(misc_TCLSCRIPTS
  rename_regions_to_ids.tcl
)
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(misc_TCLSCRIPTS tclscripts/misc)
endif (BRLCAD_ENABLE_TCL)

# We intentionally install but DO NOT INDEX these scripts!  General
# purpose scripts belong elsewhere (e.g., util).
#
#   pkgIndex_BUILD(tclscripts/misc)
#   tclIndex_BUILD(tclscripts/misc)

CMAKEFILES(
  CMakeLists.txt
  ${misc_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
