set(LDIR "${BRLCAD_SOURCE_DIR}/src/other/linenoise")

set(NIRT_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${ANALYZE_INCLUDE_DIRS}
  ${LDIR}
  )
BRLCAD_INCLUDE_DIRS(NIRT_INCLUDE_DIRS)

set(NIRT_SRCS
  main.cpp
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  )

BRLCAD_ADDEXEC(nirt "${NIRT_SRCS}" "libanalyze;librt;libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(showshot showshot.c libbn)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
