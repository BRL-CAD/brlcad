set(uv_DESCRIPTION "
Option for enabling and disabling compilation of the libuv library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(uv UV uv
  uv_DESCRIPTION
  REQUIRED_VARS BRLCAD_LEVEL2
  ALIASES ENABLE_UV
  RESET_VARS UV_LIBRARY UV_INCLUDE_DIR
  )

if (BRLCAD_UV_BUILD)

  set(UV_MAJOR_VERSION 1)
  set(UV_MINOR_VERSION 0)
  set(UV_PATCH_VERSION 0)
  set(UV_VERSION ${UV_MAJOR_VERSION}.${UV_MINOR_VERSION}.${UV_PATCH_VERSION})
  set_lib_vars(UV uv ${UV_MAJOR_VERSION} ${UV_MINOR_VERSION} ${UV_PATCH_VERSION})

  if (MSVC)
    set(UV_BASENAME uv)
    set(UV_STATICNAME uv-static)
  else (MSVC)
    set(UV_BASENAME libuv)
    set(UV_STATICNAME libuv)
  endif (MSVC)

  set(UV_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/uv)

  ExternalProject_Add(UV_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/uv"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_BINDIR=${BIN_DIR}
    -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_INSTALL_PREFIX=${UV_INSTDIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DLIB_DIR=${LIB_DIR}
    -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
    -DBUILD_TESTING=OFF
    -DLIBUV_BUILD_BENCH=OFF
    -DLIBUV_BUILD_TESTS=OFF
    -DLIBUV_BUILD_SHARED=ON
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/UV_BLD-prefix")

 # Tell the parent build about files and libraries
 ExternalProject_Target(SHARED uv UV_BLD ${UV_INSTDIR}
   ${UV_BASENAME}${UV_SUFFIX}
   SYMLINKS "${UV_SYMLINK_1};${UV_SYMLINK_2}"
   LINK_TARGET "${UV_SYMLINK_1}"
   RPATH
   )

 ExternalProject_ByProducts(uv UV_BLD ${UV_INSTDIR} ${INCLUDE_DIR}
   uv/aix.h
   uv/bsd.h
   uv/darwin.h
   uv/errno.h
   uv/linux.h
   uv/os390.h
   uv/posix.h
   uv/sunos.h
   uv/threadpool.h
   uv/tree.h
   uv/unix.h
   uv/version.h
   uv/win.h
   uv.h
   )
 set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} uv CACHE STRING "Bundled system include dirs" FORCE)

 set(UV_LIBRARY uv CACHE STRING "Building bundled uv" FORCE)
 set(UV_LIBRARIES uv CACHE STRING "Building bundled uv" FORCE)
 set(UV_INCLUDE_DIR "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing uv headers." FORCE)
 set(UV_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing uv headers." FORCE)

 SetTargetFolder(UV_BLD "Third Party Libraries")
 SetTargetFolder(uv "Third Party Libraries")

endif (BRLCAD_UV_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/uv.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

