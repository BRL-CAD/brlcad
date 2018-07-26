if(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)
  # Minimum required version of CMake
  cmake_minimum_required(VERSION 3.1.3)

  # Set CMake project name
  project(UG2G)

else(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

  include_directories(
    "${CMAKE_CURRENT_BINARY_DIR}/../../../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/other/openNURBS"
    "${ZLIB_INCLUDE_DIR}"
    "${REGEX_INCLUDE_DIR}"
    "${TCL_INCLUDE_DIRS}"
    )

  # The build shims aren't so great at mocking the initialization
  # behavior of the real code.
  CHECK_CXX_FLAG("Wno-uninitialized")
  CHECK_CXX_FLAG("Wno-maybe-uninitialized")

  set(ug_g_srcs ug-g.c ug_misc.c shim.cpp)
  add_definitions(-DTEST_BUILD)
  add_library(ug-g ${ug_g_srcs})
  target_link_libraries(ug-g librt libwdb libbrep libbg libnmg libbn libbu ${TCL_LIBRARIES})
  set_target_properties(ug-g PROPERTIES FOLDER "BRL-CAD Executables/Build Only")
  if(CPP_DLL_DEFINES)
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BU_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BN_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "NMG_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BG_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "BREP_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "RT_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "WDB_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "GDIAM_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "VDS_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "ON_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "TINYCTHREAD_DLL_IMPORTS")
    set_property(TARGET ug-g APPEND PROPERTY COMPILE_DEFINITIONS "LZ4_DLL_IMPORT=1")
  endif(CPP_DLL_DEFINES)

  # Distcheck file list
  CMAKEFILES(${ug_g_srcs} conv.h log.h shim.h ug-g.1 ug_misc.h)

endif(NOT DEFINED BRLCAD_BUILDTEST_EXTERNALS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
