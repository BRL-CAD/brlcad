if (BRLCAD_LEVEL2)

  set(PERPLEX_BLD_ROOT ${CMAKE_BINARY_DIR}/superbuild/perplex)

  ExternalProject_Add(PERPLEX_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/perplex"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PERPLEX_BLD_ROOT} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DDATA_DIR=${DATA_DIR}
    )

  # Tell the parent about files and libraries
  ExternalProject_Target(perplex_lemon PERPLEX_BLD ${PERPLEX_BLD_ROOT}
    EXEC ${BIN_DIR}/lemon${EXE_EXT}
    RPATH
    )
  ExternalProject_Target(perplex_re2c PERPLEX_BLD ${PERPLEX_BLD_ROOT}
    EXEC ${BIN_DIR}/re2c${EXE_EXT}
    RPATH
    )
  ExternalProject_Target(perplex_perplex PERPLEX_BLD ${PERPLEX_BLD_ROOT}
    EXEC ${BIN_DIR}/perplex${EXE_EXT}
    RPATH
    )
  ExternalProject_ByProducts(perplex_lemon PERPLEX_BLD ${PERPLEX_BLD_ROOT} ${DATA_DIR}/lemon ${DATA_DIR}/lemon
    lempar.c
    )
  ExternalProject_ByProducts(perplex_perplex PERPLEX_BLD ${PERPLEX_BLD_ROOT} ${DATA_DIR}/perplex ${DATA_DIR}/perplex
    perplex_template.c
    )

endif (BRLCAD_LEVEL2)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

