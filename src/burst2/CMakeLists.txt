set(LDIR "${BRLCAD_SOURCE_DIR}/src/other/linenoise")

set(BURST_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/other/linenoise
  )
list(REMOVE_DUPLICATES BURST_INCLUDE_DIRS)
include_directories(${BURST_INCLUDE_DIRS})

set(burst_SOURCES
  burst.cpp
  grid.cpp
  idents.cpp
  paint.cpp
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  )

BRLCAD_ADDEXEC(burst2 "${burst_SOURCES}" "librt;libfb;libbu;${M_LIBRARY}")

set(burst_ignore
  CMakeLists.txt
  burst.h
  burst.format
  )
CMAKEFILES(${burst_ignore})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
