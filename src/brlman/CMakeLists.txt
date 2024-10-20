set(brlman_libs libbu ${IMM32_LIBRARY} ${COMCTL32_LIBRARY})

# Tcl/Tk version of brlman GUI
if(NOT BRLCAD_ENABLE_QT AND TARGET libtclcad AND TK_LIBRARY)
  brlcad_adddata(brlman.tcl tclscripts/brlman)

  set(brlman_libs libtclcad ${TCL_LIBRARY} ${TK_LIBRARY} ${brlman_libs})

  if(TARGET Tkhtml)
    set(brlman_libs ${brlman_libs} Tkhtml)
  endif(TARGET Tkhtml)
endif(NOT BRLCAD_ENABLE_QT AND TARGET libtclcad AND TK_LIBRARY)

# Qt version of brlman GUI
if(BRLCAD_ENABLE_QT)
  find_package_qt(REQUIRED)

  if(Qt6Widgets_FOUND)
    set(brlman_libs libqtcad Qt6::Widgets Qt6::Gui ${brlman_libs})
  else()
    set(brlman_libs libqtcad Qt5::Widgets Qt5::Gui ${brlman_libs})
  endif(Qt6Widgets_FOUND)
endif(BRLCAD_ENABLE_QT)

# Building "GUI" doesn't matter except on Windows, but
# on Windows brlman works only in graphical mode - might
# as well behave "nicely" there.
brlcad_addexec(brlman brlman.cpp "${brlman_libs}" GUI)
if(BRLCAD_ENABLE_QT)
  target_compile_definitions(brlman PRIVATE USE_QT)
endif(BRLCAD_ENABLE_QT)
if(TK_LIBRARY)
  target_compile_definitions(brlman PRIVATE HAVE_TK)
endif(TK_LIBRARY)

if(NOT BRLCAD_ENABLE_QT AND TARGET brlman_tcl_cp)
  add_dependencies(brlman brlman_tcl_cp)
  add_dependencies(brlman TclIndexBld)
endif(NOT BRLCAD_ENABLE_QT AND TARGET brlman_tcl_cp)

if(TARGET TclIndexBld)
  add_dependencies(brlman TclIndexBld)
endif(TARGET TclIndexBld)

cmakefiles(
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
