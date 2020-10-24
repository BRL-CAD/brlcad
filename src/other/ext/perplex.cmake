if (BRLCAD_LEVEL2)

  set(PERPLEX_BLD_ROOT ${CMAKE_BINARY_DIR}/other/ext/perplex)

  ExternalProject_Add(PERPLEX_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/perplex"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PERPLEX_BLD_ROOT} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DDATA_DIR=${DATA_DIR}
    )

  # Tell the parent about files and libraries
  ExternalProject_Target(EXEC perplex_lemon PERPLEX_BLD ${PERPLEX_BLD_ROOT}
    lemon${CMAKE_EXECUTABLE_SUFFIX}
    RPATH
    )
  ExternalProject_Target(EXEC perplex_re2c PERPLEX_BLD ${PERPLEX_BLD_ROOT}
    re2c${CMAKE_EXECUTABLE_SUFFIX}
    RPATH
    )
  ExternalProject_Target(EXEC perplex_perplex PERPLEX_BLD ${PERPLEX_BLD_ROOT}
    perplex${CMAKE_EXECUTABLE_SUFFIX}
    RPATH
    )
  ExternalProject_ByProducts(perplex_lemon PERPLEX_BLD ${PERPLEX_BLD_ROOT} ${DATA_DIR}/lemon
    lempar.c
    )
  ExternalProject_ByProducts(perplex_perplex PERPLEX_BLD ${PERPLEX_BLD_ROOT} ${DATA_DIR}/perplex
    perplex_template.c
    )

  set(LEMON_TEMPLATE "${CMAKE_BINARY_ROOT}/${DATA_DIR}/lemon/lempar.c" CACHE PATH "lemon template" FORCE)
  set(PERPLEX_TEMPLATE "${CMAKE_BINARY_ROOT}/${DATA_DIR}/perplex/perplex_template.c" CACHE PATH "perplex template" FORCE)
  set(LEMON_EXECUTABLE perplex_lemon CACHE STRING "lemon" FORCE)
  set(RE2C_EXECUTABLE perplex_re2c CACHE STRING "re2c" FORCE)
  set(PERPLEX_EXECUTABLE perplex_perplex CACHE STRING "perplex" FORCE)

endif (BRLCAD_LEVEL2)

include("${CMAKE_CURRENT_SOURCE_DIR}/perplex.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

