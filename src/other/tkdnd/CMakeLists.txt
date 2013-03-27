# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(tkdnd)

SET(CMAKE_MODULE_PATH
  "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/CMake")
find_package(TCL)
find_package(X11)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

## ===========================================================================
##  Project Information
## ===========================================================================
PROJECT(tkdnd)
## Package version information:
set(PKG_NAME ${PROJECT_NAME} )
set(PKG_MAJOR_VERSION 2 )
set(PKG_MINOR_VERSION 6 )
set(PKG_BUILD_VERSION 0 )
## Author:
set(PKG_VENDOR        "Georgios Petasis" )

## Arange project version information...
set(PKG_VERSION "${PKG_MAJOR_VERSION}.${PKG_MINOR_VERSION}" )
set(PKG_NAME_VERSION  ${PKG_NAME}-${PKG_VERSION} )

## Package global definitions...
ADD_DEFINITIONS   ( -DPACKAGE_NAME="${PKG_NAME}" )
ADD_DEFINITIONS   ( -DPACKAGE_VERSION="${PKG_VERSION}" )
ADD_DEFINITIONS   ( -DPACKAGE_NAME_VERSION="${PKG_NAME_VERSION}" )
ADD_DEFINITIONS   ( -DTKDND_PACKAGE="${PKG_NAME}" )
ADD_DEFINITIONS   ( -DTKDND_VERSION="${PKG_VERSION}" )

## Package debug definitions...
# ADD_DEFINITIONS   ( -DDEBUG_CLIENTMESSAGE_HANDLER )

## ===========================================================================
##  Project sources
## ===========================================================================
if(WIN32)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win)
  set(PKG_SOURCES win/TkDND_OleDND.cpp)
  add_definitions(-DUNICODE )
  add_definitions(-D_UNICODE )
endif(WIN32)
if(APPLE)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/macosx)
  set(PKG_SOURCES unix/macosx/macdnd.m)
  add_definitions(-DMAC_TK_COCOA)
  add_definitions(-std=gnu99)
  add_definitions(-x objective-c)
  add_definitions(-fobjc-gc)
  set(SYSTEM_LIBRARIES -framework Cocoa -framework Carbon)
endif(APPLE)
if(NOT APPLE AND NOT WIN32)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/unix)
  include_directories(${X11_INCLUDE_DIR})
  set(PKG_SOURCES unix/TkDND_XDND.c unix/tkUnixSelect.c unix/Cursors.c)
  set(SYSTEM_LIBRARIES ${X11_LIBRARIES})
endif(NOT APPLE AND NOT WIN32)

##
## If we are building with MINGW, link statically to its libs...
##
if(MINGW)
  include(CheckCCompilerFlag)
  CHECK_C_COMPILER_FLAG(-static-libgcc    FLAG_static_libgcc    )
  CHECK_C_COMPILER_FLAG(-static-libstdc++ FLAG_static_libstdcpp )
  if(FLAG_static_libgcc )
    set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc" )
  endif(FLAG_static_libgcc )
  if(FLAG_static_libstdcpp )
    set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++" )
  endif(FLAG_static_libstdcpp )
endif(MINGW )

## ===========================================================================
##  Tcl/Tk
## ===========================================================================

## Enable Tcl/Tk stubs globally...
add_definitions(-DUSE_TCL_STUBS)
add_definitions(-DUSE_TK_STUBS)
add_definitions(-DTCL_THREADS)
include_directories(${TCL_INCLUDE_PATH} ${TK_INCLUDE_PATH})

if(WIN32 AND NO_MSVCRT)
  string(REPLACE /MD  /MT  CMAKE_C_FLAGS_RELEASE   ${CMAKE_C_FLAGS_RELEASE}  )
  string(REPLACE /MDd /MTd CMAKE_C_FLAGS_DEBUG     ${CMAKE_C_FLAGS_DEBUG}    )
  string(REPLACE /MD  /MT  CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
  string(REPLACE /MDd /MTd CMAKE_CXX_FLAGS_DEBUG   ${CMAKE_CXX_FLAGS_DEBUG}  )
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /nodefaultlib:MSVCRT.LIB" )
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} LIBCMT.LIB" )
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} LIBCMTD.LIB" )
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /nodefaultlib:MSVCRT.LIB" )
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} LIBCMT.LIB" )
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /nodefaultlib:LIBCMT.LIB LIBCMTD.LIB" )
  add_definitions(-D_STATIC_CPPLIB )
  add_definitions(-D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1 )
  add_definitions(-D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1 )
  add_definitions(-D_CRT_SECURE_NO_WARNINGS )
endif(WIN32 AND NO_MSVCRT )

add_library(tkdnd SHARED ${PKG_SOURCES})
target_link_libraries(tkdnd ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY} ${SYSTEM_LIBRARIES})
set_target_properties(tkdnd PROPERTIES VERSION ${PKG_VERSION})
install(TARGETS tkdnd
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

set(TKDND_MISC
  library/tkdnd.tcl
  library/tkdnd_compat.tcl
  library/tkdnd_macosx.tcl
  library/tkdnd_unix.tcl
  library/tkdnd_windows.tcl
  )

if(CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    if(MSVC)
      set(libdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}")
    else(MSVC)
      set(libdir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}")
    endif(MSVC)
    get_target_property(target_LIBLOCATION tkdnd LOCATION_${CFG_TYPE_UPPER})
    get_filename_component(target_LIBNAME ${target_LIBLOCATION} NAME)
    configure_file(pkgIndex.tcl.in ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}/${PKG_NAME}${PKG_VERSION}/pkgIndex.tcl)
    DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}}/${pkgname}${pkgversion})
    file(COPY ${TKDND_MISC} DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${LIB_DIR}/${PKG_NAME}${PKG_VERSION})
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  if(MSVC)
    set(libdir "${CMAKE_INSTALL_PREFIX}/${BIN_DIR}")
  else(MSVC)
    set(libdir "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}")
  endif(MSVC)
  configure_file(pkgIndex.tcl.in ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl)
else(CMAKE_CONFIGURATION_TYPES)
  if(CMAKE_BUILD_TYPE)
    get_target_property(target_LIBLOCATION tkdnd LOCATION_${CMAKE_BUILD_TYPE})
  else(CMAKE_BUILD_TYPE)
    get_target_property(target_LIBLOCATION tkdnd LOCATION)
  endif(CMAKE_BUILD_TYPE)
  get_filename_component(target_LIBNAME ${target_LIBLOCATION} NAME)
  set(libdir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  configure_file(pkgIndex.tcl.in ${libdir}/${PKG_NAME}${PKG_VERSION}/pkgIndex.tcl)
  set(libdir "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}")
  configure_file(pkgIndex.tcl.in ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl)
  file(COPY ${TKDND_MISC} DESTINATION ${CMAKE_BINARY_DIR}/${LIB_DIR}/${PKG_NAME}${PKG_VERSION})
endif(CMAKE_CONFIGURATION_TYPES)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION ${LIB_DIR}/${PKG_NAME}${PKG_VERSION})

install(FILES ${TKDND_MISC} DESTINATION ${LIB_DIR}/${PKG_NAME}${PKG_VERSION})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

