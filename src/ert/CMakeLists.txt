set(RTBIN_INCLUDE_DIRS
  ${OPTICAL_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${EMBREE_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
list(REMOVE_DUPLICATES RTBIN_INCLUDE_DIRS)
include_directories(${RTBIN_INCLUDE_DIRS})

set(ert_SRCS
  ert.c
  )

BRLCAD_ADDEXEC(ert "${ert_SRCS}" "librt;libfb;liboptical;libicv;${EMBREE_LIBRARIES}" NO_INSTALL)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
