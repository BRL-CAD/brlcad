set(BRLMAN_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES BRLMAN_INCLUDE_DIRS)
include_directories(${BRLMAN_INCLUDE_DIRS})

BRLCAD_ADDDATA(brlman.tcl tclscripts/brlman)

set(brlman_libs libtclcad libbu ${TCL_LIBRARIES})

if(TARGET Tkhtml)
  set(brlman_libs ${brlman_libs} Tkhtml)
endif(TARGET Tkhtml)

if(CPP_DLL_DEFINES)
  set(brlman_libs ${brlman_libs} imm32.lib comctl32.lib)
endif(CPP_DLL_DEFINES)

# Building "GUI" doesn't matter except on Windows, but
# on Windows brlman works only in graphical mode - might
# as well behave "nicely" there.
BRLCAD_ADDEXEC(brlman brlman.c "${brlman_libs}" GUI)
add_dependencies(brlman brlman_tcl_cp)

foreach(item ${tclindex_target_list})
  add_dependencies(brlman ${item})
endforeach(item ${tclindex_target_list})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
