set(FREECAD_SRCS fcstd_document.cpp freecad_read.cpp occt_brep.cpp)
cmakefiles(CMakeLists.txt ${FREECAD_SRCS} fcstd_document.h occt_brep.h
  test_freecad.cpp test_freecad.cmake)

set(freecad_occt_components COMPONENTS FoundationClasses ModelingData)
if(BUILD_TESTING)
  # Geometry construction toolkits are needed only to generate the fixture.
  list(APPEND freecad_occt_components OPTIONAL_COMPONENTS ModelingAlgorithms)
endif()
brlcad_find_package(OpenCASCADE CONFIG QUIET ${freecad_occt_components})
if(NOT OpenCASCADE_FOUND OR NOT TARGET TKBRep OR NOT TARGET TKGeomBase)
  return()
endif()

brlcad_find_package(expat CONFIG QUIET)
if(TARGET expat::expat)
  set(freecad_expat expat::expat)
else()
  brlcad_find_package(EXPAT MODULE QUIET)
  if(NOT TARGET EXPAT::EXPAT)
    return()
  endif()
  set(freecad_expat EXPAT::EXPAT)
endif()

brlcad_find_package(minizip-ng CONFIG QUIET)
if(TARGET MINIZIP::minizip-ng)
  # Some system builds omit the compatibility API used by the reader.
  include(CheckSymbolExists)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES MINIZIP::minizip-ng)
  unset(FREECAD_MINIZIP_NG_COMPAT CACHE)
  check_symbol_exists(unzOpen64 "unzip.h" FREECAD_MINIZIP_NG_COMPAT)
  cmake_pop_check_state()
  if(FREECAD_MINIZIP_NG_COMPAT)
    set(freecad_minizip MINIZIP::minizip-ng)
  endif()
endif()
if(NOT freecad_minizip)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(FREECAD_MINIZIP QUIET IMPORTED_TARGET minizip)
    if(TARGET PkgConfig::FREECAD_MINIZIP)
      set(freecad_minizip PkgConfig::FREECAD_MINIZIP)
    endif()
  endif()
endif()
if(NOT freecad_minizip)
  find_package(LibArchive QUIET)
  if(NOT TARGET LibArchive::LibArchive)
    return()
  endif()
endif()

if(freecad_minizip)
  set(freecad_zip ${freecad_minizip})
else()
  set(freecad_zip LibArchive::LibArchive)
endif()

gcv_plugin_library(gcv-freecad SHARED ${FREECAD_SRCS})
target_include_directories(gcv-freecad SYSTEM PRIVATE "${OpenCASCADE_INCLUDE_DIR}")
target_compile_definitions(gcv-freecad PRIVATE BRLCADBUILD HAVE_CONFIG_H)
target_link_libraries(gcv-freecad
  libgcv
  libwdb
  librt
  libbg
  libbn
  libbu
  OPENNURBS::OPENNURBS
  TKBRep
  TKGeomBase
  ${freecad_expat}
  ${freecad_zip}
)
if(freecad_minizip)
  target_compile_definitions(gcv-freecad PRIVATE GCV_FREECAD_MINIZIP)
endif()
validate_style(gcv-freecad "fcstd_document.cpp;freecad_read.cpp")
plugin_setup(gcv-freecad gcv)

if(BUILD_TESTING AND TARGET TKPrim AND TARGET TKTopAlgo)
  brlcad_addexec(test_gcv_freecad test_freecad.cpp
    "librt;libbu;OPENNURBS::OPENNURBS;TKPrim;TKTopAlgo" TEST)
  target_include_directories(test_gcv_freecad SYSTEM PRIVATE "${OpenCASCADE_INCLUDE_DIR}")
  foreach(freecad_test IN ITEMS gcv_freecad gcv_freecad_arrays gcv_freecad_links)
    brlcad_add_test(NAME ${freecad_test} COMMAND ${CMAKE_COMMAND}
      -DGCV=$<TARGET_FILE:gcv>
      -DFIXTURE=$<TARGET_FILE:test_gcv_freecad>
      -DCASE=${freecad_test}
      -DWORK=${CMAKE_CURRENT_BINARY_DIR}/test/${freecad_test}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/test_freecad.cmake)
  endforeach()
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
