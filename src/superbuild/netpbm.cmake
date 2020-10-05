set(netpbm_DESCRIPTION "
Option for enabling and disabling compilation of the netpbm library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(netpbm NETPBM netpbm netpbm_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL2 ALIASES ENABLE_NETPBM)

if (BRLCAD_NETPBM_BUILD)

  if (MSVC)
    set(NETPBM_BASENAME netpbm)
  else (MSVC)
    set(NETPBM_BASENAME libnetpbm)
  endif (MSVC)

  ExternalProject_Add(NETPBM_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/netpbm"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(netpbm NETPBM_BLD
    OUTPUT_FILE ${NETPBM_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${NETPBM_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(NETPBM_BLD ${INCLUDE_DIR}
    netpbm/bitio.h
    netpbm/colorname.h
    netpbm/pam.h
    netpbm/pammap.h
    netpbm/pbm.h
    netpbm/pbmfont.h
    netpbm/pgm.h
    netpbm/pm.h
    netpbm/pm_gamma.h
    netpbm/pm_system.h
    netpbm/pnm.h
    netpbm/ppm.h
    netpbm/ppmcmap.h
    netpbm/ppmfloyd.h
    )

  list(APPEND BRLCAD_DEPS NETPBM_BLD)

  set(NETPBM_LIBRARIES netpbm CACHE STRING "Building bundled netpbm" FORCE)
  set(NETPBM_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/${INCLUDE_DIR}/netpbm" CACHE STRING "Directory containing netpbm headers." FORCE)

  SetTargetFolder(NETPBM_BLD "Third Party Libraries")
  SetTargetFolder(netpbm "Third Party Libraries")

endif (BRLCAD_NETPBM_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

