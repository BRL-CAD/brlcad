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

THIRD_PARTY(png PNG png png_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL2 ALIASES ENABLE_PNG)

if (BRLCAD_PNG_BUILD)

  set(PNG_VERSION_MAJOR 16)
  set(PNG_VERSION_MINOR 37)
  set(PNG_LIB_NAME png_brl)

  if (MSVC)
    set(PNG_BASENAME ${PNG_LIB_NAME})
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(PNG_BASENAME lib${PNG_LIB_NAME})
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR}.${PNG_VERSION_MINOR}.0)
  endif (MSVC)

  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
  endif (TARGET ZLIB_BLD)

  set(PNG_INSTDIR ${CMAKE_BINARY_DIR}/png)

  ExternalProject_Add(PNG_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/png"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PNG_INSTDIR} -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DSKIP_INSTALL_EXPORT=ON
	       -DPNG_STATIC=${BUILD_STATIC_LIBS} -DSKIP_INSTALL_EXECUTABLES=ON -DSKIP_INSTALL_FILES=ON
               -DSKIP_INSTALL_EXPORT=ON -DPNG_TESTS=OFF -Dld-version-script=OFF
	       -DZLIB_ROOT=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}/$<CONFIG>> -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}/$<CONFIG>/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}>
               -DPNG_LIB_NAME=${PNG_LIB_NAME} -DPNG_PREFIX=brl_
    DEPENDS ${ZLIB_TARGET}
    )

  # Tell the parent build about files and libraries
  ExternalProject_Target(png PNG_BLD ${PNG_INSTDIR}
    SHARED ${LIB_DIR}/${PNG_BASENAME}${PNG_SUFFIX}
    STATIC ${LIB_DIR}/${PNG_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS ${LIB_DIR}/${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${LIB_DIR}/${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR};${LIB_DIR}/${PNG_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    LINK_TARGET ${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_LINK_TARGET ${PNG_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )
  ExternalProject_ByProducts(png PNG_BLD ${PNG_INSTDIR} ${INCLUDE_DIR} ${INCLUDE_DIR}
    png.h
    pngconf.h
    pnglibconf.h
    libpng${PNG_VERSION_MAJOR}/png.h
    libpng${PNG_VERSION_MAJOR}/pngconf.h
    libpng${PNG_VERSION_MAJOR}/pnglibconf.h
    )

  set(PNG_LIBRARIES png CACHE STRING "Building bundled libpng" FORCE)
  set(PNG_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/${INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)

  SetTargetFolder(PNG_BLD "Third Party Libraries")
  SetTargetFolder(png "Third Party Libraries")

endif (BRLCAD_PNG_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

