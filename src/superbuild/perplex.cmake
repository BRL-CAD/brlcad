ExternalProject_Add(PERPLEX_BLD
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/perplex"
  BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DBIN_DIR=${BIN_DIR}
  -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DDATA_DIR=${DATA_DIR}
  )

set(LEMON_TEMPLATE ${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/lemon/lempar.c CACHE STRING "Lemon template file" FORCE)
set(PERPLEX_TEMPLATE "${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/perplex/perplex_template.c" CACHE STRING "Perplex template file" FORCE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

