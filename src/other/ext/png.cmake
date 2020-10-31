# NOTE: we need to have libpng's internal call to find_package looking for zlib
# locate our local copy if we have one.  Defining the ZLIB_ROOT prefix for
# find_package and setting the library file to our custom library name is
# intended to do this (requires CMake 3.12).

set(png_DESCRIPTION "
Option for enabling and disabling compilation of the Portable Network
Graphics library provided with BRL-CAD's source distribution.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")

# We generally don't want the Mac framework libpng...
set(CMAKE_FIND_FRAMEWORK LAST)

THIRD_PARTY(png PNG png
  png_DESCRIPTION
  REQUIRED_VARS BRLCAD_LEVEL2
  ALIASES ENABLE_PNG
  RESET_VARS PNG_LIBRARY_DEBUG PNG_LIBRARY_RELEASE
  )

if (BRLCAD_PNG_BUILD)

  set(PNG_VERSION_MAJOR 16)
  set(PNG_VERSION_MINOR 37)
  set(PNG_LIB_NAME png_brl)

  if (MSVC)
    set(PNG_BASENAME ${PNG_LIB_NAME})
    set(PNG_STATICNAME ${PNG_LIB_NAME}_static)
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(PNG_BASENAME lib${PNG_LIB_NAME})
    set(PNG_STATICNAME lib${PNG_LIB_NAME})
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR}.${PNG_VERSION_MINOR}.0)
  endif (MSVC)

  set(PNG_DEPS)
  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
    list(APPEND PNG_DEPS ZLIB_BLD zlib_stage)
  endif (TARGET ZLIB_BLD)

  set(PNG_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/png)

  if (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}.lib)
  else (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif (MSVC)

  ExternalProject_Add(PNG_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/png"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PNG_INSTDIR} -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DSKIP_INSTALL_EXPORT=ON
	       -DPNG_STATIC=${BUILD_STATIC_LIBS} -DSKIP_INSTALL_EXECUTABLES=ON -DSKIP_INSTALL_FILES=ON
               -DSKIP_INSTALL_EXPORT=ON -DPNG_TESTS=OFF -Dld-version-script=OFF
	       -DZLIB_ROOT=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_ROOT}>
	       -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
	       $<$<BOOL:${ZLIB_TARGET}>:-DZ_PREFIX=ON>
	       $<$<BOOL:${ZLIB_TARGET}>:-DZ_PREFIX_STR=${Z_PREFIX_STR}>
               -DPNG_LIB_NAME=${PNG_LIB_NAME} -DPNG_PREFIX=brl_
    DEPENDS ${PNG_DEPS}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/PNG_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED png PNG_BLD ${PNG_INSTDIR}
    ${PNG_BASENAME}${PNG_SUFFIX}
    SYMLINKS ${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR}
    LINK_TARGET ${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    RPATH
    )
  if (BUILD_STATIC_LIBS)
    ExternalProject_Target(STATIC png-static PNG_BLD ${PNG_INSTDIR}
      ${PNG_STATICNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      SYMLINKS ${PNG_STATICNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      )
  endif (BUILD_STATIC_LIBS)

  ExternalProject_ByProducts(png PNG_BLD ${PNG_INSTDIR} ${INCLUDE_DIR}
    png.h
    pngconf.h
    pnglibconf.h
    libpng${PNG_VERSION_MAJOR}/png.h
    libpng${PNG_VERSION_MAJOR}/pngconf.h
    libpng${PNG_VERSION_MAJOR}/pnglibconf.h
    )

  set(PNG_LIBRARY_DEBUG png CACHE STRING "Building bundled libpng" FORCE)
  set(PNG_LIBRARY_RELEASE png CACHE STRING "Building bundled libpng" FORCE)
  set(PNG_LIBRARIES png CACHE STRING "Building bundled libpng" FORCE)
  set(PNG_PNG_INCLUDE_DIR "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)
  set(PNG_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)

  SetTargetFolder(PNG_BLD "Third Party Libraries")
  SetTargetFolder(png "Third Party Libraries")

else (BRLCAD_PNG_BUILD)

  set(PNG_INCLUDE_DIRS "${PNG_PNG_INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)

  if (DEFINED PNG_LIBRARY_RELEASE)
    set(PNG_LIBRARIES ${PNG_LIBRARY_RELEASE} CACHE STRING "libpng" FORCE)
  else (DEFINED PNG_LIBRARY_RELEASE)
    set(PNG_LIBRARIES ${PNG_LIBRARY_DEBUG} CACHE STRING "libpng" FORCE)
  endif (DEFINED PNG_LIBRARY_RELEASE)

endif (BRLCAD_PNG_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/png.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

