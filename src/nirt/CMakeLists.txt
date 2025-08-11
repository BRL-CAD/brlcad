brlcad_addexec(nirt main.cpp "libanalyze;librt;libbu;${LINENOISE_LIBRARIES};${M_LIBRARY}")
target_include_directories(nirt BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../libtermio)
set_property(TARGET nirt APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")

brlcad_addexec(showshot showshot.c libbn)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
