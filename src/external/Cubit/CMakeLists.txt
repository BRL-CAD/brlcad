if(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)
  # Minimum required version of CMake
  cmake_minimum_required(VERSION 2.8)
  if(COMMAND CMAKE_POLICY)
    CMAKE_POLICY(SET CMP0003 NEW)
    if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
      CMAKE_POLICY(SET CMP0026 OLD)
    endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
  endif(COMMAND CMAKE_POLICY)

  # Set CMake project name
  project(G2SAT)

else(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

  include_directories(
    "${CMAKE_CURRENT_BINARY_DIR}/../../../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/other/openNURBS"
    "${ZLIB_INCLUDE_DIR}"
    "${REGEX_INCLUDE_DIR}"
    )

  set(g_sat_srcs g-sat.cpp)
  add_definitions(-DTEST_BUILD)
  add_library(g-sat ${g_sat_srcs})
  target_link_libraries(g-sat librt libwdb libbrep libbg libnmg libbn libbu)
  set_target_properties(g-sat PROPERTIES FOLDER "BRL-CAD Executables/Build Only")
  if(CPP_DLL_DEFINES)
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "BU_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "BN_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "NMG_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "BG_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "BREP_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "RT_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "WDB_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "GDIAM_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "VDS_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "ON_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "TINYCTHREAD_DLL_IMPORTS")
    set_property(TARGET g-sat APPEND PROPERTY COMPILE_DEFINITIONS "LZ4_DLL_IMPORT=1")
  endif(CPP_DLL_DEFINES)

endif(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
