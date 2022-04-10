set(bv_headers
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
  view_sets.h
  )
BRLCAD_MANAGE_FILES(bv_headers ${INCLUDE_DIR}/brlcad/bv)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
