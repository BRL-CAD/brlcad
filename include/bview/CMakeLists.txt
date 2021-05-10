set(bview_headers
  adc.h
  defines.h
  events.h
  faceplate.h
  tcl_data.h
  util.h
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
