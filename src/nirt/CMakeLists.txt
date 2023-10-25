set(NIRT_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${ANALYZE_INCLUDE_DIRS}
  ${LINENOISE_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(NIRT_INCLUDE_DIRS)

BRLCAD_ADDEXEC(nirt main.cpp "libanalyze;librt;libbu;${LINENOISE_LIBRARIES};${M_LIBRARY}")
set_property(TARGET nirt APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")

BRLCAD_ADDEXEC(showshot showshot.c libbn)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
