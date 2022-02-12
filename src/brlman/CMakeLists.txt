set(BRLMAN_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  )
set(brlman_libs libbu ${IMM32_LIBRARY} ${COMCTL32_LIBRARY})

# Tcl/Tk version of brlman GUI
if (NOT BRLCAD_ENABLE_QT AND TARGET libtclcad)
  set(BRLMAN_INCLUDE_DIRS
    ${BRLMAN_INCLUDE_DIRS}
    ${TCLCAD_INCLUDE_DIRS}
    ${TCL_INCLUDE_PATH}
    )
  if (TARGET Tkhtml)
    set(BRLMAN_INCLUDE_DIRS ${BRLMAN_INCLUDE_DIRS} ${TK_INCLUDE_PATH})
  endif (TARGET Tkhtml)

  BRLCAD_ADDDATA(brlman.tcl tclscripts/brlman)

  set(brlman_libs libtclcad ${TCL_LIBRARY} ${brlman_libs})

  if(TARGET Tkhtml)
    set(brlman_libs ${brlman_libs} Tkhtml)
  endif(TARGET Tkhtml)
endif (NOT BRLCAD_ENABLE_QT AND TARGET libtclcad)

# Qt version of brlman GUI
if (BRLCAD_ENABLE_QT)
  if (Qt6Widgets_FOUND)
    set(brlman_libs libqtcad Qt6::Widgets Qt6::Gui ${brlman_libs})
  else()
    set(brlman_libs libqtcad Qt5::Widgets Qt5::Gui ${brlman_libs})
  endif (Qt6Widgets_FOUND)
endif (BRLCAD_ENABLE_QT)

list(REMOVE_DUPLICATES BRLMAN_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(BRLMAN_INCLUDE_DIRS)


# Building "GUI" doesn't matter except on Windows, but
# on Windows brlman works only in graphical mode - might
# as well behave "nicely" there.
BRLCAD_ADDEXEC(brlman brlman.cpp "${brlman_libs}" GUI)
if (BRLCAD_ENABLE_QT)
  target_compile_definitions(brlman PRIVATE USE_QT)
endif (BRLCAD_ENABLE_QT)

if (NOT BRLCAD_ENABLE_QT AND TARGET brlman_tcl_cp)
  add_dependencies(brlman brlman_tcl_cp)
  foreach(item ${tclindex_target_list})
    add_dependencies(brlman ${item})
  endforeach(item ${tclindex_target_list})
endif (NOT BRLCAD_ENABLE_QT AND TARGET brlman_tcl_cp)

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
