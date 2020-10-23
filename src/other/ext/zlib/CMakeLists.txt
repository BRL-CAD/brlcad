cmake_minimum_required(VERSION 3.12)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)

project(zlib C)

set(VERSION "1.2.11")

if(NOT DEFINED BUILD_SHARED_LIBS)
  option(BUILD_SHARED_LIBS "Build a shared library form of zlib" ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)
if(NOT DEFINED BUILD_STATIC_LIBS)
  option(BUILD_STATIC_LIBS "Build a static library form of zlib" ON)
endif(NOT DEFINED BUILD_STATIC_LIBS)

if(NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif(NOT DEFINED BIN_DIR)
if(NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif(NOT DEFINED LIB_DIR)
if(NOT DEFINED INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT DEFINED INCLUDE_DIR)

include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
enable_testing()

check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(stdint.h    HAVE_STDINT_H)
check_include_file(stddef.h    HAVE_STDDEF_H)

#
# Check to see if we have large file support
#
set(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1)
# We add these other definitions here because CheckTypeSize.cmake
# in CMake 2.4.x does not automatically do so and we want
# compatibility with CMake 2.4.x.
if(HAVE_SYS_TYPES_H)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_SYS_TYPES_H)
endif()
if(HAVE_STDINT_H)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDINT_H)
endif()
if(HAVE_STDDEF_H)
  list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDDEF_H)
endif()
check_type_size(off64_t OFF64_T)
if(HAVE_OFF64_T)
  add_definitions(-D_LARGEFILE64_SOURCE=1)
endif()
set(CMAKE_REQUIRED_DEFINITIONS) # clear variable

#
# Check for fseeko
#
check_function_exists(fseeko HAVE_FSEEKO)
if(NOT HAVE_FSEEKO)
  add_definitions(-DNO_FSEEKO)
endif()

#
# Check for unistd.h
#
check_include_file(unistd.h Z_HAVE_UNISTD_H)
if(Z_HAVE_UNISTD_H)
  add_definitions(-DZ_HAVE_UNISTD_H)
endif(Z_HAVE_UNISTD_H)

option(ENABLE_Z_PREFIX "Add z_ prefix to all library functions." OFF)
if (DEFINED Z_PREFIX_STR)
  set (ENABLE_Z_PREFIX ON CACHE BOOL "Ensure prefix is set" FORCE)
endif (DEFINED Z_PREFIX_STR)
if (ENABLE_Z_PREFIX)
  set(Z_PREFIX 1)
endif (ENABLE_Z_PREFIX)
mark_as_advanced (ENABLE_Z_PREFIX)
mark_as_advanced (Z_PREFIX_STR)

if(MSVC)
  #set(CMAKE_DEBUG_POSTFIX "d")
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zconf.h.in ${CMAKE_CURRENT_BINARY_DIR}/zconf.h @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib.h.in  ${CMAKE_CURRENT_BINARY_DIR}/zlib.h  @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

#============================================================================
# zlib
#============================================================================

set(ZLIB_PUBLIC_HDRS
  ${CMAKE_CURRENT_BINARY_DIR}/zconf.h
  ${CMAKE_CURRENT_BINARY_DIR}/zlib.h
  )
set(ZLIB_PRIVATE_HDRS
  crc32.h
  deflate.h
  gzguts.h
  inffast.h
  inffixed.h
  inflate.h
  inftrees.h
  trees.h
  zutil.h
  )
set(ZLIB_SRCS
  adler32.c
  compress.c
  crc32.c
  deflate.c
  gzclose.c
  gzlib.c
  gzread.c
  gzwrite.c
  inflate.c
  infback.c
  inftrees.c
  inffast.c
  trees.c
  uncompr.c
  zutil.c
  )

# parse the full version number from zlib.h and include in ZLIB_FULL_VERSION
file(READ ${CMAKE_CURRENT_BINARY_DIR}/zlib.h _zlib_h_contents)
string(REGEX REPLACE ".*#define[ \t]+ZLIB_VERSION[ \t]+\"([-0-9A-Za-z.]+)\".*"
  "\\1" ZLIB_FULL_VERSION ${_zlib_h_contents})

if(MINGW)
  # This gets us DLL resource information when compiling on MinGW.
  if(NOT CMAKE_RC_COMPILER)
    set(CMAKE_RC_COMPILER windres.exe)
  endif()

  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj
    COMMAND ${CMAKE_RC_COMPILER}
    -D GCC_WINDRES
    -I ${CMAKE_CURRENT_SOURCE_DIR}
    -I ${CMAKE_CURRENT_BINARY_DIR}
    -o ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj
    -i ${CMAKE_CURRENT_SOURCE_DIR}/win32/zlib1.rc)
  set(ZLIB_SRCS ${ZLIB_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/zlib1rc.obj)
else(MINGW)
  set(ZLIB_SRCS ${ZLIB_SRCS} win32/zlib1.rc)
endif(MINGW)

add_library(zlib SHARED ${ZLIB_SRCS} ${ZLIB_ASMS} ${ZLIB_DLL_SRCS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
set_target_properties(zlib PROPERTIES OUTPUT_NAME z_brl)

if(BUILD_STATIC_LIBS)
  add_library(zlib-static STATIC ${ZLIB_SRCS} ${ZLIB_ASMS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
  set_target_properties(zlib-static PROPERTIES OUTPUT_NAME z_brl)
  if(CMAKE_CL_64)
    set_target_properties(zlib-static PROPERTIES STATIC_LIBRARY_FLAGS "/machine:x64")
  elseif(MSVC)
    set_target_properties(zlib-static PROPERTIES STATIC_LIBRARY_FLAGS "/machine:x86")
  endif(CMAKE_CL_64)
endif(BUILD_STATIC_LIBS)

set_target_properties(zlib PROPERTIES DEFINE_SYMBOL ZLIB_DLL)
set_target_properties(zlib PROPERTIES SOVERSION 1)

if(NOT CYGWIN)
  # This property causes shared libraries on Linux to have the full version
  # encoded into their final filename.  We disable this on Cygwin because
  # it causes cygz-${ZLIB_FULL_VERSION}.dll to be created when cygz.dll
  # seems to be the default.
  #
  # This has no effect with MSVC, on that platform the version info for
  # the DLL comes from the resource file win32/zlib1.rc
  set_target_properties(zlib PROPERTIES VERSION ${ZLIB_FULL_VERSION})
endif()

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL )
  if(BUILD_SHARED_LIBS)
    install(TARGETS zlib
      RUNTIME DESTINATION "${BIN_DIR}"
      ARCHIVE DESTINATION "${LIB_DIR}"
      LIBRARY DESTINATION "${LIB_DIR}" )
  endif(BUILD_SHARED_LIBS)
  if(BUILD_STATIC_LIBS)
    install(TARGETS zlib-static
      RUNTIME DESTINATION "${BIN_DIR}"
      ARCHIVE DESTINATION "${LIB_DIR}"
      LIBRARY DESTINATION "${LIB_DIR}" )
  endif(BUILD_STATIC_LIBS)
endif()
if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL )
  install(FILES ${ZLIB_PUBLIC_HDRS} DESTINATION ${INCLUDE_DIR})
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
