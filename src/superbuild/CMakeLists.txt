include(ExternalProject)
ExternalProject_Add(BRLCAD_BLD
  SOURCE_DIR "${BRLCAD_SOURCE_DIR}"
  PREFIX ${BRLCAD_BINARY_DIR}/brlcad
  BINARY_DIR ${BRLCAD_BINARY_DIR}/brlcad-build
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
  -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
  -DSUBBUILD=ON
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

