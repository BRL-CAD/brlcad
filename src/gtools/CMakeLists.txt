if (BRLCAD_COMPONENTS OR NOT EXISTS ${CMAKE_SOURCE_DIR}/src/other)
  find_package_linenoise(REQUIRED)
endif (BRLCAD_COMPONENTS OR NOT EXISTS ${CMAKE_SOURCE_DIR}/src/other)

BRLCAD_ADDDATA(gtransfer.c sample_applications)

BRLCAD_ADDEXEC(ganalyze ganalyze.cpp "libanalyze;librt;libbu;libpkg;${LINENOISE_LIBRARIES}" NO_INSTALL)
target_include_directories(ganalyze BEFORE PRIVATE ${LINENOISE_INCLUDE_DIRS})
set_property(TARGET ganalyze APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")

BRLCAD_ADDEXEC(gchecker gchecker.cpp "libged;librt;libbu")
BRLCAD_ADDEXEC(gex "gex.cpp" "libbu;librt")
BRLCAD_ADDEXEC(glint glint.cpp "librt;${M_LIBRARY}")
BRLCAD_ADDEXEC(gqa gqa.c "libged;librt")

BRLCAD_ADDEXEC(gsh gsh.cpp  "libged;libdm;libbu;${LINENOISE_LIBRARIES}")
target_include_directories(gsh BEFORE PRIVATE ${LINENOISE_INCLUDE_DIRS})
set_property(TARGET gsh APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")
add_dependencies(gsh dm_plugins)

BRLCAD_ADDEXEC(gtransfer gtransfer.c "librt;libpkg" NO_INSTALL)

add_subdirectory(beset)
add_subdirectory(gdiff)
add_subdirectory(gist)
add_subdirectory(tests)

CMAKEFILES(
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
