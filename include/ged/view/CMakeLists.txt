set(ged_headers
  adc.h
  matrix.h
  select.h
  state.h
  )

install(FILES ${ged_headers} DESTINATION ${INCLUDE_DIR}/brlcad/ged/view)

CMAKEFILES(${ged_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
