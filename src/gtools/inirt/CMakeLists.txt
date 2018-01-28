set(LDIR "${BRLCAD_SOURCE_DIR}/src/other/linenoise")

set(INIRT_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${ANALYZE_INCLUDE_DIRS}
  ${LDIR}
  )
BRLCAD_INCLUDE_DIRS(INIRT_INCLUDE_DIRS)

set(inirt_srcs
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  main.cxx
  )

BRLCAD_ADDEXEC(inirt "${inirt_srcs}" "libanalyze;librt;libbu")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
