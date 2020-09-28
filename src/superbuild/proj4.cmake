set(proj4_DESCRIPTION "
Option for enabling and disabling compilation of the PROJ.4 geographic
projection library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(proj-4 PROJ4 proj4 proj4_DESCRIPTION REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2" ALIASES ENABLE_PROJ4)

if (${CMAKE_PROJECT_NAME}_PROJ4_BUILD)

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

  ExternalProject_Add(PROJ4_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/proj-4"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
               -DPROJ_LIB_DIR=${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/proj
    )
  ExternalProject_Target(proj PROJ4_BLD
    OUTPUT_FILE ${PROJ_BASENAME}${PROJ_SUFFIX}
    STATIC_OUTPUT_FILE ${PROJ_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_API_VERSION}"
    LINK_TARGET "${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "${PROJ_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )

  ExternalProject_ByProducts(PROJ4_BLD ${DATA_DIR}
    proj/epsg
    proj/esri
    proj/world
    proj/esri.extra
    proj/other.extra
    proj/IGNF
    proj/nad27
    proj/GL27
    proj/nad83
    proj/nad.lst
    proj/proj_def.dat
    proj/CH
    )

  ExternalProject_ByProducts(PROJ4_BLD ${INCLUDE_DIR}/proj
    projects.h
    proj_api.h
    geodesic.h
    )

  list(APPEND BRLCAD_DEPS PROJ4_BLD)

  set(PROJ4_LIBRARIES proj CACHE STRING "Building bundled proj" FORCE)
  set(PROJ4_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/proj" CACHE STRING "Directory containing proj headers." FORCE)

  SetTargetFolder(PROJ4_BLD "Third Party Libraries")
  SetTargetFolder(proj "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_PROJ4_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

