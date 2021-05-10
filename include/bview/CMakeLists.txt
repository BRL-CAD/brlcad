set(bview_headers
  adc.h
  defines.h
  events.h
  faceplate.h
  plot3.h
  tcl_data.h
  tig.h
  util.h
  vectfont.h
  vlist.h
  )
BRLCAD_MANAGE_FILES(bview_headers ${INCLUDE_DIR}/brlcad/bview)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
