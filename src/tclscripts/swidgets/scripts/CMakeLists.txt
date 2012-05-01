set(swidgets_scripts_TCLSCRIPTS
  selectlists.itk
  tkgetdir.itk
  togglearrow.itk
  tooltip.itk
  tree.itk
  )
BRLCAD_ADDDATA(swidgets_scripts_TCLSCRIPTS tclscripts/swidgets/scripts)
tclIndex_BUILD(swidgets_scripts tclscripts/swidgets/scripts)

CMAKEFILES(tclIndex)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
