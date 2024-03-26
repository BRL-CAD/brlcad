BRLCAD_ADDEXEC(nirt main.cpp "libanalyze;librt;libbu;${LINENOISE_LIBRARIES};${M_LIBRARY}")
target_include_directories(nirt BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${LINENOISE_INCLUDE_DIRS})
set_property(TARGET nirt APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")

BRLCAD_ADDEXEC(showshot showshot.c libbn)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
