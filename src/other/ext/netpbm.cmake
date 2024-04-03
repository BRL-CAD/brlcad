set(netpbm_DESCRIPTION "
Option for enabling and disabling compilation of the netpbm library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(netpbm NETPBM netpbm
  netpbm_DESCRIPTION
  ALIASES ENABLE_NETPBM
  RESET_VARS NETPBM_LIBRARY NETPBM_INCLUDE_DIR
  )

if (BRLCAD_NETPBM_BUILD)

  if (MSVC)
    set(NETPBM_BASENAME netpbm)
    set(NETPBM_STATICNAME netpbm-static)
  else (MSVC)
    set(NETPBM_BASENAME libnetpbm)
    set(NETPBM_STATICNAME libnetpbm)
  endif (MSVC)

  set(NETPBM_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/netpbm)

  ExternalProject_Add(NETPBM_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/netpbm"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBIN_DIR=${BIN_DIR}
    -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_INSTALL_PREFIX=${NETPBM_INSTDIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DLIB_DIR=${LIB_DIR}
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/NETPBM_BLD-prefix")

 # Tell the parent build about files and libraries
 ExternalProject_Target(SHARED netpbm NETPBM_BLD ${NETPBM_INSTDIR}
   ${NETPBM_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
   RPATH
   )
 if (BUILD_STATIC_LIBS)
   ExternalProject_Target(STATIC netpbm-static NETPBM_BLD ${NETPBM_INSTDIR}
     ${NETPBM_STATICNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
     )
 endif (BUILD_STATIC_LIBS)

 ExternalProject_ByProducts(netpbm NETPBM_BLD ${NETPBM_INSTDIR} ${INCLUDE_DIR}/netpbm
   bitio.h
   colorname.h
   pam.h
   pammap.h
   pbm.h
   pbmfont.h
   pgm.h
   pm.h
   pm_gamma.h
   pnm.h
   ppm.h
   ppmcmap.h
   ppmfloyd.h
   pm_config.h
   )
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} netpbm  CACHE STRING "Bundled system include dirs" FORCE)

  set(NETPBM_LIBRARY netpbm CACHE STRING "Building bundled netpbm" FORCE)
  set(NETPBM_LIBRARIES netpbm CACHE STRING "Building bundled netpbm" FORCE)
  set(NETPBM_INCLUDE_DIR "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/netpbm" CACHE STRING "Directory containing netpbm headers." FORCE)
  set(NETPBM_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/netpbm" CACHE STRING "Directory containing netpbm headers." FORCE)

  SetTargetFolder(NETPBM_BLD "Third Party Libraries")
  SetTargetFolder(netpbm "Third Party Libraries")

endif (BRLCAD_NETPBM_BUILD)

mark_as_advanced(NETPBM_INCLUDE_DIRS)
mark_as_advanced(NETPBM_LIBRARIES)

include("${CMAKE_CURRENT_SOURCE_DIR}/netpbm.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

