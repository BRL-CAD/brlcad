if (TARGET libtclcad)
set(BRLMAN_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${TCL_INCLUDE_PATH}
  )
if (TARGET Tkhtml)
  set(BRLMAN_INCLUDE_DIRS ${BRLMAN_INCLUDE_DIRS} ${TK_INCLUDE_PATH})
endif (TARGET Tkhtml)
list(REMOVE_DUPLICATES BRLMAN_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(BRLMAN_INCLUDE_DIRS)

BRLCAD_ADDDATA(brlman.tcl tclscripts/brlman)

set(brlman_libs libtclcad libbu ${TCL_LIBRARY} ${IMM32_LIBRARY} ${COMCTL32_LIBRARY})

if(TARGET Tkhtml)
  set(brlman_libs ${brlman_libs} Tkhtml)
endif(TARGET Tkhtml)

# Building "GUI" doesn't matter except on Windows, but
# on Windows brlman works only in graphical mode - might
# as well behave "nicely" there.
BRLCAD_ADDEXEC(brlman brlman.cpp "${brlman_libs}" GUI)
add_dependencies(brlman brlman_tcl_cp)

foreach(item ${tclindex_target_list})
  add_dependencies(brlman ${item})
endforeach(item ${tclindex_target_list})
endif (TARGET libtclcad)

CMAKEFILES(
  CMakeLists.txt
  brlman.cpp
  brlman.tcl
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
