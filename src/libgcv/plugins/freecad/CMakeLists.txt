# FreeCAD FCStd importer/exporter plugin for libgcv.
#
# This plugin intentionally does not bundle OpenCASCADE.  It is compiled only
# when an external OpenCASCADE package, BRL-CAD's openNURBS, and zlib are
# available.  FCStd ZIP archive I/O uses the local zlib-backed helper in this
# directory.

set(FREECAD_SRCS freecad_read.cpp freecad_write.cpp)
set(FREECAD_EXTRA_SRCS
  fcstd_zip.cpp
  fcstd_zip.h
  open2open/include/open2open/brep_convert.h
  open2open/include/open2open/fcstd_convert.h
  open2open/include/open2open/geom_convert.h
  open2open/include/open2open/mesh_convert.h
  open2open/src/brep_convert.cpp
  open2open/src/fcstd_convert.cpp
  open2open/src/geom_convert.cpp
  open2open/src/mesh_convert.cpp
)

set(_OPEN2OPEN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/open2open")
if(NOT EXISTS "${_OPEN2OPEN_SOURCE_DIR}/include/open2open/fcstd_convert.h" OR NOT EXISTS "${_OPEN2OPEN_SOURCE_DIR}/src/fcstd_convert.cpp")
  message(STATUS "gcv-freecad: local open2open source copy is incomplete - FreeCAD plugin disabled")
  cmakefiles(${FREECAD_SRCS} CMakeLists.txt)
  return()
endif()

set(_FREECAD_OCCT_BUILD_TREE FALSE)
if(OpenCASCADE_DIR)
  get_filename_component(_FREECAD_OCCT_CONFIG_DIR "${OpenCASCADE_DIR}" ABSOLUTE)
  get_filename_component(_FREECAD_OCCT_SOURCE_DIR "${_FREECAD_OCCT_CONFIG_DIR}" DIRECTORY)

  if(EXISTS "${_FREECAD_OCCT_CONFIG_DIR}/OpenCASCADEConfig.cmake"
      AND EXISTS "${_FREECAD_OCCT_CONFIG_DIR}/lin64/clang/lib"
      AND EXISTS "${_FREECAD_OCCT_SOURCE_DIR}/src/Standard/Standard.hxx")
    set(OpenCASCADE_FOUND TRUE)
    set(_FREECAD_OCCT_BUILD_TREE TRUE)
    set(OpenCASCADE_LIBRARY_DIR "${_FREECAD_OCCT_CONFIG_DIR}/lin64/clang/lib")
  endif()
endif()

if(NOT OpenCASCADE_FOUND)
  find_package(OpenCASCADE CONFIG QUIET)
  if(NOT OpenCASCADE_FOUND)
    find_package(OpenCASCADE QUIET)
  endif()
endif()

if(NOT OpenCASCADE_FOUND)
  message(STATUS "gcv-freecad: OpenCASCADE not found - FreeCAD plugin disabled")
  cmakefiles(${FREECAD_SRCS} CMakeLists.txt)
  return()
endif()

find_package(ZLIB REQUIRED)
if(TARGET ZLIB::ZLIB)
  set(_FREECAD_ZLIB_LIB ZLIB::ZLIB)
else()
  set(_FREECAD_ZLIB_LIB ${ZLIB_LIBRARIES})
endif()

brlcad_find_package(OPENNURBS REQUIRED SYSPATTERN openNURBS)
if(NOT TARGET OPENNURBS::OPENNURBS)
  message(STATUS "gcv-freecad: BRL-CAD openNURBS target not found - FreeCAD plugin disabled")
  cmakefiles(${FREECAD_SRCS} CMakeLists.txt)
  return()
endif()

set(_FREECAD_OCCT_LIBS
  TKernel
  TKMath
  TKG2d
  TKG3d
  TKGeomBase
  TKBRep
  TKGeomAlgo
  TKTopAlgo
  TKCDF
)

set(_FREECAD_OCCT_INCLUDE_DIRS ${OpenCASCADE_INCLUDE_DIR})
set(_FREECAD_OCCT_LIBRARY_DIRS ${OpenCASCADE_LIBRARY_DIR})

if(OpenCASCADE_DIR)
  get_filename_component(_FREECAD_OCCT_CONFIG_DIR "${OpenCASCADE_DIR}" ABSOLUTE)

  if(EXISTS "${_FREECAD_OCCT_CONFIG_DIR}/lin64/clang/lib")
    list(APPEND _FREECAD_OCCT_LIBRARY_DIRS "${_FREECAD_OCCT_CONFIG_DIR}/lin64/clang/lib")
  endif()

  if(EXISTS "${_FREECAD_OCCT_CONFIG_DIR}/inc")
    list(APPEND _FREECAD_OCCT_INCLUDE_DIRS "${_FREECAD_OCCT_CONFIG_DIR}/inc")
  endif()

  get_filename_component(_FREECAD_OCCT_SOURCE_DIR "${_FREECAD_OCCT_CONFIG_DIR}" DIRECTORY)
  if(EXISTS "${_FREECAD_OCCT_SOURCE_DIR}/src/Standard/Standard.hxx")
    file(GLOB _FREECAD_OCCT_SRC_INCLUDE_DIRS LIST_DIRECTORIES true "${_FREECAD_OCCT_SOURCE_DIR}/src/*")
    list(APPEND _FREECAD_OCCT_INCLUDE_DIRS ${_FREECAD_OCCT_SRC_INCLUDE_DIRS})
  endif()
endif()

list(REMOVE_DUPLICATES _FREECAD_OCCT_INCLUDE_DIRS)
list(REMOVE_DUPLICATES _FREECAD_OCCT_LIBRARY_DIRS)

set(_FREECAD_OPEN2OPEN_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/fcstd_zip.cpp"
  "${_OPEN2OPEN_SOURCE_DIR}/src/geom_convert.cpp"
  "${_OPEN2OPEN_SOURCE_DIR}/src/brep_convert.cpp"
  "${_OPEN2OPEN_SOURCE_DIR}/src/mesh_convert.cpp"
  "${_OPEN2OPEN_SOURCE_DIR}/src/fcstd_convert.cpp"
)

add_library(gcv_freecad_open2open OBJECT ${_FREECAD_OPEN2OPEN_SRCS})
target_include_directories(gcv_freecad_open2open
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${_OPEN2OPEN_SOURCE_DIR}/include"
    "${_OPEN2OPEN_SOURCE_DIR}/compat"
    ${_FREECAD_OCCT_INCLUDE_DIRS}
)
target_compile_definitions(gcv_freecad_open2open PUBLIC OPEN2OPEN_HAVE_LIBZIP)
target_link_libraries(gcv_freecad_open2open PUBLIC OPENNURBS::OPENNURBS ${_FREECAD_ZLIB_LIB})
target_compile_options(gcv_freecad_open2open PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-w>)

message(STATUS "gcv-freecad: FreeCAD plugin enabled using open2open at ${_OPEN2OPEN_SOURCE_DIR}")

gcv_plugin_library(gcv-freecad SHARED ${FREECAD_SRCS})
target_include_directories(gcv-freecad PRIVATE
  $<TARGET_PROPERTY:gcv_freecad_open2open,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(gcv-freecad PRIVATE BRLCADBUILD HAVE_CONFIG_H OPEN2OPEN_HAVE_LIBZIP)
if(_FREECAD_OCCT_LIBRARY_DIRS)
  target_link_directories(gcv-freecad PRIVATE ${_FREECAD_OCCT_LIBRARY_DIRS})
endif()
target_link_libraries(gcv-freecad
  libgcv
  librt
  libwdb
  libbg
  libbu
  OPENNURBS::OPENNURBS
  $<TARGET_OBJECTS:gcv_freecad_open2open>
  ${_FREECAD_OCCT_LIBS}
  ${_FREECAD_ZLIB_LIB}
)
validate_style(gcv-freecad "${FREECAD_SRCS}")
plugin_setup(gcv-freecad gcv)

cmakefiles(
  ${FREECAD_SRCS}
  ${FREECAD_EXTRA_SRCS}
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
