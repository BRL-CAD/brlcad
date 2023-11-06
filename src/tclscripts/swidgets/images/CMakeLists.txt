set(swidgets_image_DATA
  closed.gif
  leaf.gif
  minus.xbm
  open.gif
  plus.xbm
  tip.gif
  )
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(swidgets_image_DATA tclscripts/swidgets/images)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${swidgets_image_DATA}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
