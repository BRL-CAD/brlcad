if(BRLCAD_ENABLE_QT)
  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${GED_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    ${Qt5Widgets_INCLUDE_DIRS}
    )

  add_definitions(${Qt5Widgets_DEFINITIONS})
endif(BRLCAD_ENABLE_QT)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qged_srcs
  main.cxx
  main_window.cxx
  cadaccordian.cxx
  cadapp.cxx
  cadappexec.cxx
  cadattributes.cxx
  cadcommands.cxx
  cadconsole.cxx
  cadhelp.cxx
  cadimport.cxx
  cadtreemodel.cxx
  cadtreeview.cxx
  cadtreenode.cxx
  cadview.cxx
  console.cxx
  pqConsoleWidget.cxx
  QAccordianWidget.cxx
  QToolPalette.cxx
  QFlowLayout.cxx
  )

set(moc_headers
  main_window.h
  cadaccordian.h
  cadapp.h
  cadappexec.h
  cadattributes.h
  cadcommands.h
  cadhelp.h
  cadimport.h
  cadconsole.h
  cadtreemodel.h
  cadtreeview.h
  cadview.h
  console.h
  pqConsoleWidget.h
  QAccordianWidget.h
  QToolPalette.h
  QFlowLayout.h
  )

if(BRLCAD_ENABLE_QT)
  QT5_WRAP_CPP(moc_srcs ${moc_headers})
  QT5_ADD_RESOURCES(cad_qrc cadresources.qrc)

  add_executable(qged ${qged_srcs} ${moc_srcs} ${cad_qrc})
  qt5_use_modules(qged Widgets OpenGL)
  target_link_libraries(qged libbu librt libged ${QT_LIBRARIES} ${OPENGL_LIBRARIES})
  install(TARGETS qged
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
else(BRLCAD_ENABLE_QT)
  CMAKEFILES(${qged_srcs})
  CMAKEFILES(${moc_headers})
endif(BRLCAD_ENABLE_QT)

set(qged_ignore_srcs
  default.qss
  cadstyle.qss
  cadtreestyle.qss
  pqConsoleWidget_LICENSE.txt
  fonts/Inconsolata-LICENSE.txt
  fonts/Inconsolata.otf
  images/primitives/other.png
  images/primitives/dsp.png
  images/primitives/ars.png
  images/primitives/eto.png
  images/primitives/region.png
  images/primitives/tgc.png
  images/primitives/extrude.png
  images/primitives/part.png
  images/primitives/tor.png
  images/primitives/ehy.png
  images/primitives/arb8.png
  images/primitives/air.png
  images/primitives/arbn.png
  images/primitives/arb5.png
  images/primitives/rpc.png
  images/primitives/nmg.png
  images/primitives/assembly.png
  images/primitives/pipe.png
  images/primitives/sketch.png
  images/primitives/invalid.png
  images/primitives/ell.png
  images/primitives/sph.png
  images/primitives/arb6.png
  images/primitives/brep.png
  images/primitives/comb.png
  images/primitives/epa.png
  images/primitives/bot.png
  images/primitives/half.png
  images/primitives/rhc.png
  images/primitives/metaball.png
  images/primitives/arb7.png
  images/primitives/hyp.png
  images/primitives/arb4.png
  images/primitives/airregion.png
  images/appleseed/scrollbar_arrow_up_disabled.png
  images/appleseed/combobox_arrow_down_disabled.png
  images/appleseed/radiobutton_unchecked_enabled.png
  images/appleseed/radiobutton_checked_disabled.png
  images/appleseed/spinbox_arrow_down_enabled.png
  images/appleseed/spinbox_arrow_up_disabled.png
  images/appleseed/menu_arrow_right_selected.png
  images/appleseed/scrollbar_arrow_left_enabled.png
  images/appleseed/radiobutton_checked_enabled_hover.png
  images/appleseed/scrollbar_arrow_right_enabled.png
  images/appleseed/radiobutton_unchecked_enabled_hover.png
  images/appleseed/checkbox_indeterminate_disabled.png
  images/appleseed/checkbox_checked_enabled.png
  images/appleseed/scrollbar_arrow_up_enabled.png
  images/appleseed/scrollbar_arrow_left_disabled.png
  images/appleseed/scrollbar_arrow_down_enabled.png
  images/appleseed/scrollbar_arrow_right_disabled.png
  images/appleseed/header_arrow_up_enabled.png
  images/appleseed/checkbox_unchecked_disabled.png
  images/appleseed/combobox_arrow_down_selected.png
  images/appleseed/scrollbar_arrow_down_disabled.png
  images/appleseed/radiobutton_unchecked_disabled.png
  images/appleseed/header_arrow_down_disabled.png
  images/appleseed/menu_arrow_right_disabled.png
  images/appleseed/checkbox_unchecked_enabled.png
  images/appleseed/combobox_arrow_down_enabled.png
  images/appleseed/header_arrow_up_disabled.png
  images/appleseed/radiobutton_checked_enabled.png
  images/appleseed/checkbox_indeterminate_enabled.png
  images/appleseed/header_arrow_down_enabled.png
  images/appleseed/spinbox_arrow_up_enabled.png
  images/appleseed/LICENSE.txt
  images/appleseed/menu_arrow_right_enabled.png
  images/appleseed/spinbox_arrow_down_disabled.png
  images/appleseed/checkbox_checked_disabled.png
  images/tree/branch-end.png
  images/tree/branch-open.png
  images/tree/branch-closed.png
  images/tree/branch-more.png
  images/tree/vline.png
  )

CMAKEFILES(${qged_ignore_srcs})

  # Local Variables:
  # tab-width: 8
  # mode: cmake
  # indent-tabs-mode: t
  # End:
  # ex: shiftwidth=2 tabstop=8

