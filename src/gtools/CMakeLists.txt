brlcad_find_package(LINENOISE REQUIRED)

brlcad_adddata(gtransfer.c sample_applications)

brlcad_addexec(ganalyze ganalyze.cpp "libanalyze;librt;libbu;libpkg;${LINENOISE_LIBRARIES}" NO_INSTALL)
target_include_directories(ganalyze BEFORE PRIVATE ${LINENOISE_INCLUDE_DIRS})
set_property(TARGET ganalyze APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")

brlcad_addexec(gchecker gchecker.cpp "libged;librt;libbu")
add_dependencies(gchecker ged-check)
brlcad_addexec(gex "gex.cpp" "libbu;librt")
brlcad_addexec(glint glint.cpp "librt;${M_LIBRARY}")
brlcad_addexec(gqa gqa.c "libged;librt")

brlcad_addexec(gtransfer gtransfer.c "librt;libpkg" NO_INSTALL)

add_subdirectory(beset)
add_subdirectory(gdiff)
add_subdirectory(gist)
add_subdirectory(gsh)
add_subdirectory(tests)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
