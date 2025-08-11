set(
  swidgets_scripts_TCLSCRIPTS
  selectlists.itk
  tkgetdir.itk
  togglearrow.itk
  tooltip.itk
  tree.itk
)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(swidgets_scripts_TCLSCRIPTS tclscripts/swidgets/scripts)
  tclindex_build(tclscripts/swidgets/scripts)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${swidgets_scripts_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
