# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# Set CMake project name
PROJECT(POPT)

set(PACKAGE popt)
set(VERSION 1.16)
set(PACKAGE_VERSION ${VERSION})
set(POPT_SYSCONFDIR /etc/conf)

#---------------------------------------------------------------------
# Configure install locations.  Don't set these if they have already
# been set by some other means (like a higher level CMakeLists.txt file
# including this one.)

# The location in which to install executables.
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

# The location in which to install header files.
if(NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT INCLUDE_DIR)

# The location in which to install libraries.
if(NOT LIB_DIR)
  if(NOT WIN32)
    set(LIB_DIR lib)
  else(NOT WIN32)
    set(LIB_DIR bin)
  endif(NOT WIN32)
endif(NOT LIB_DIR)

# The location in which to install Manual pages
if(NOT MAN_DIR)
  set(MAN_DIR man)
endif(NOT MAN_DIR)


include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(float.h HAVE_FLOAT_H)
CHECK_INCLUDE_FILES(fnmatch.h HAVE_FNMATCH_H)
CHECK_INCLUDE_FILES(glob.h HAVE_GLOB_H)
CHECK_INCLUDE_FILES(langinfo.h HAVE_LANGINFO_H)
CHECK_INCLUDE_FILES(libintl.h HAVE_LIBINTL_H)
CHECK_INCLUDE_FILES(mcheck.h HAVE_MCHECK_H)
CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)

include(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(setreuid HAVE_SETREUID)
CHECK_FUNCTION_EXISTS(getuid HAVE_GETUID)
CHECK_FUNCTION_EXISTS(geteuid HAVE_GETEUID)
CHECK_FUNCTION_EXISTS(iconv HAVE_ICONV)
CHECK_FUNCTION_EXISTS(mtrace HAVE_MTRACE)
CHECK_FUNCTION_EXISTS(__set_getenv HAVE___SET_GETENV)
CHECK_FUNCTION_EXISTS(setregid HAVE_SETREGID)
CHECK_FUNCTION_EXISTS(stpcpy HAVE_STPCPY)
CHECK_FUNCTION_EXISTS(strerror HAVE_STRERROR)
CHECK_FUNCTION_EXISTS(vasprintf HAVE_VASPRINTF)
CHECK_FUNCTION_EXISTS(srandom HAVE_SRANDOM)

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_definitions(-DHAVE_CONFIG_H)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(POPT_SRCS popt.c poptparse.c poptconfig.c popthelp.c poptint.c)
set(UNUSED_SRCS lookup3.c)
set(TEST_SRCS test1.c test2.c tdict.c)

set(POPT_HDRS popt.h)
set(POPT_NOINST_HDRS poptint.h system.h)

add_library(libpopt SHARED ${POPT_SRCS})


install(TARGETS libpopt
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(MSVC)
  set_property(TARGET libpopt APPEND PROPERTY COMPILE_DEFINITIONS "POPT_DLL_EXPORTS")
endif(MSVC)

if(BUILD_STATIC_LIBS)
  add_library(libpopt-static STATIC ${POPT_SRCS})
  if(CMAKE_CL_64)
    set_target_properties(libpopt-static PROPERTIES STATIC_LIBRARY_FLAGS "/machine:x64")
  endif(CMAKE_CL_64)
endif(BUILD_STATIC_LIBS)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
