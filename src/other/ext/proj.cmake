set(proj_DESCRIPTION "
Option for enabling and disabling compilation of the PROJ geographic
projection library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(proj PROJ proj
  proj_DESCRIPTION
  REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2"
  ALIASES ENABLE_PROJ
  RESET_VARS PROJ_LIBRARY PROJ_LIBRARIES PROJ_INCLUDE_DIR PROJ_INCLUDE_DIRS
  )

if (BRLCAD_PROJ_BUILD)

  set(PROJ_MAJOR_VERSION 9)
  set(PROJ_MINOR_VERSION 2)
  set(PROJ_API_VERSION 25)
  set(PROJ_VERSION ${PROJ_API_VERSION}.${PROJ_MAJOR_VERSION}.${PROJ_MINOR_VERSION}.0)

  # Our logic needs to know what files we are looking for to incorporate into
  # our build, and unfortunately those names are not only platform specific but
  # also project specific - they can (and do) define their own conventions, so
  # we have to define these uniquely for each situation.
  if (MSVC)
    set(PROJ_BASENAME proj)
    set(PROJ_SUFFIX _${PROJ_MAJOR_VERSION}_${PROJ_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PROJ_SYMLINK_1 ${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PROJ_SYMLINK_2 ${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_API_VERSION})
  elseif (APPLE)
    set(PROJ_BASENAME libproj)
    set(PROJ_SUFFIX .${PROJ_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PROJ_SYMLINK_1 ${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PROJ_SYMLINK_2 ${PROJ_BASENAME}.${PROJ_API_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (OPENBSD)
    set(PROJ_BASENAME libproj)
    set(PROJ_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_MAJOR_VERSION}.${PROJ_MINOR_VERSION})
  else (MSVC)
    set(PROJ_BASENAME libproj)
    set(PROJ_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_VERSION})
    set(PROJ_SYMLINK_1 ${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PROJ_SYMLINK_2 ${PROJ_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_API_VERSION})
  endif (MSVC)

  # Unfortunately, because PROJ is a separate build, the custom import targets we have
  # defined can't be passed to the PROJ build to identify these components - we need
  # actual file paths.  We also need to depend on the staging targets that will actually
  # place these files where we're telling the PROJ build they will be.
  set(PROJ_DEPS)
  if (TARGET SQLITE3_BLD)
    set(SQLITE3_TARGET SQLITE3_BLD)
    list(APPEND PROJ_DEPS SQLITE3_BLD libsqlite3_stage sqlite3_exe_stage)
    if (MSVC)
      set(SQLite3_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/libsqlite3.lib)
    elseif (OPENBSD)
      set(SQLite3_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/libsqlite3${CMAKE_SHARED_LIBRARY_SUFFIX}.3.32)
    else (MSVC)
      set(SQLite3_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/libsqlite3${CMAKE_SHARED_LIBRARY_SUFFIX})
    endif (MSVC)
    set(SQLite3_INCLUDE_DIR ${CMAKE_BINARY_ROOT}/${INCLUDE_DIR})
    set(SQLite3_EXECNAME ${CMAKE_BINARY_ROOT}/${BIN_DIR}/sqlite3${CMAKE_EXECUTABLE_SUFFIX})
  endif (TARGET SQLITE3_BLD)

  set(PROJ_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/proj)

  ExternalProject_Add(PROJ_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proj"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBIN_DIR=${BIN_DIR}
    -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_INSTALL_PREFIX=${PROJ_INSTDIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DLIB_DIR=${LIB_DIR}
    -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
    -DPROJ_LIB_DIR=${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/proj
    -DSQLITE3_LIBRARY=${SQLite3_LIBRARY}
    -DSQLITE3_INCLUDE_DIR=${SQLite3_INCLUDE_DIR}
    -DEXE_SQLITE3=${SQLite3_EXECNAME}
    -DBUILD_TESTING=OFF
    -DENABLE_TIFF=OFF
    -DENABLE_CURL=OFF
    -DBUILD_PROJSYNC=OFF
    DEPENDS ${PROJ_DEPS}
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/PROJ_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED proj PROJ_BLD ${PROJ_INSTDIR}
    ${PROJ_BASENAME}${PROJ_SUFFIX}
    SYMLINKS ${PROJ_SYMLINK_1};${PROJ_SYMLINK_2}
    LINK_TARGET ${PROJ_SYMLINK_1}
    RPATH
    WIN_DIF_LIB_NAME ${PROJ_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )

  ExternalProject_ByProducts(proj PROJ_BLD ${PROJ_INSTDIR} ${DATA_DIR}/proj
    CH
    GL27
    ITRF2000
    ITRF2008
    ITRF2014
    deformation_model.schema.json
    nad.lst
    nad27
    nad83
    other.extra
    proj.db
    proj.ini
    projjson.schema.json
    triangulation.schema.json
    world
    )

  ExternalProject_ByProducts(proj PROJ_BLD ${PROJ_INSTDIR} ${INCLUDE_DIR}
    geodesic.h
    proj.h
    proj_constants.h
    proj_experimental.h
    proj_symbol_rename.h
    )

  ExternalProject_ByProducts(proj PROJ_BLD ${PROJ_INSTDIR} ${INCLUDE_DIR}/proj
    common.hpp
    crs.hpp
    metadata.hpp
    io.hpp
    nn.hpp
    datum.hpp
    coordinatesystem.hpp
    util.hpp
    coordinateoperation.hpp
    )
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} proj  CACHE STRING "Bundled system include dirs" FORCE)


  set(PROJ_LIBRARIES proj CACHE STRING "Building bundled proj" FORCE)
  set(PROJ_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}/proj" CACHE STRING "Directory containing proj headers." FORCE)

  SetTargetFolder(PROJ_BLD "Third Party Libraries")
  SetTargetFolder(proj "Third Party Libraries")

endif (BRLCAD_PROJ_BUILD)

mark_as_advanced(PROJ_INCLUDE_DIRS)
mark_as_advanced(PROJ_LIBRARIES)

include("${CMAKE_CURRENT_SOURCE_DIR}/proj.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

