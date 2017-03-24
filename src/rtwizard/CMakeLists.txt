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

BRLCAD_ADDEXEC(rtwizard "${RTWIZARD_SRCS}" "libtclcad;librt;libfb;liboptical;libicv;${ITCL_LIBRARY};${ITK_LIBRARY};${CMAKE_THREAD_LIBS_INIT};${M_LIBRARY}")
ADD_TARGET_DEPS(rtwizard rt rtedge fbserv fb-png fb-pix fbclear fblabel fbfree fbline fb-fb pix-bw bwmod bw-pix pixmatte pix-fb tkpng)
foreach(item ${tclindex_target_list})
  add_dependencies(rtwizard ${item})
endforeach(item ${tclindex_target_list})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
