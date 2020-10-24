set(proj4_DESCRIPTION "
Option for enabling and disabling compilation of the PROJ.4 geographic
projection library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(proj-4 PROJ4 proj4
  proj4_DESCRIPTION
  REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2"
  ALIASES ENABLE_PROJ4
  RESET_VARS PROJ4_LIBRARY PROJ4_LIBRARIES PROJ4_INCLUDE_DIR PROJ4_INCLUDE_DIRS
  )

if (BRLCAD_PROJ4_BUILD)

  set(PROJ_MAJOR_VERSION 4)
  set(PROJ_MINOR_VERSION 9)
  set(PROJ_API_VERSION 12)
  set(PROJ_VERSION ${PROJ_MAJOR_VERSION}.${PROJ_MINOR_VERSION}.${PROJ_API_VERSION})

  if (MSVC)
    set(PROJ_BASENAME proj)
    set(PROJ_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(PROJ_BASENAME libproj)
    set(PROJ_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_VERSION})
  endif (MSVC)

  set(PROJ4_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/proj-4)

  ExternalProject_Add(PROJ4_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proj-4"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PROJ4_INSTDIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
               -DPROJ_LIB_DIR=${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/proj
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/PROJ4_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED proj PROJ4_BLD ${PROJ4_INSTDIR}
    ${PROJ_BASENAME}${PROJ_SUFFIX}
    SYMLINKS ${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_API_VERSION}
    LINK_TARGET ${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    RPATH
    )
  ExternalProject_Target(STATIC proj-static PROJ4_BLD ${PROJ4_INSTDIR}
    ${PROJ_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )

  ExternalProject_ByProducts(proj PROJ4_BLD ${PROJ4_INSTDIR} ${DATA_DIR}/proj
    epsg
    esri
    world
    esri.extra
    other.extra
    IGNF
    nad27
    GL27
    nad83
    nad.lst
    proj_def.dat
    CH
    )

  ExternalProject_ByProducts(proj PROJ4_BLD ${PROJ4_INSTDIR} ${INCLUDE_DIR}/proj
    projects.h
    proj_api.h
    geodesic.h
    )
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} proj  CACHE STRING "Bundled system include dirs" FORCE)


  set(PROJ4_LIBRARIES proj CACHE STRING "Building bundled proj" FORCE)
  set(PROJ4_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/proj" CACHE STRING "Directory containing proj headers." FORCE)

  SetTargetFolder(PROJ4_BLD "Third Party Libraries")
  SetTargetFolder(proj "Third Party Libraries")

endif (BRLCAD_PROJ4_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/proj-4.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

