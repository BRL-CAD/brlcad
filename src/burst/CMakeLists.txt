# Until the new code is proved out, keep the original around
# as burst and name the new binary nburst
add_subdirectory(old)

set(LDIR "${BRLCAD_SOURCE_DIR}/src/other/linenoise")

set(BURST_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LDIR}
  )
list(REMOVE_DUPLICATES BURST_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(BURST_INCLUDE_DIRS)

set(burst_SOURCES
  burst.cpp
  grid.cpp
  idents.cpp
  paint.cpp
  ${LDIR}/utf8.c
  ${LDIR}/linenoise.c
  )

BRLCAD_ADDEXEC(nburst "${burst_SOURCES}" "librt;libdm;libbu;${M_LIBRARY}")
ADD_TARGET_DEPS(nburst dm_plugins)

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
