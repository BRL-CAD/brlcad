set(RTWIZARD_INCLUDE_DIRS
  ${OPTICAL_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
LIST(REMOVE_DUPLICATES RTWIZARD_INCLUDE_DIRS)
include_directories(${RTWIZARD_INCLUDE_DIRS})

set(RTWIZARD_SRCS
  main.c
  )

BRLCAD_ADDEXEC(rtwiz "${RTWIZARD_SRCS}" "libtclcad;librt;libfb;liboptical;libicv;${ITCL_LIBRARY};${ITK_LIBRARY};${CMAKE_THREAD_LIBS_INIT};${M_LIBRARY}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
