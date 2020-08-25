set(PLOT_SRCS
  dm-plot.c
  )


if(BRLCAD_ENABLE_TCL)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${TCL_INCLUDE_PATH}
    )

  add_definitions(-DDM_PLUGIN)

  dm_plugin_library(dm-plot SHARED ${PLOT_SRCS})
  target_link_libraries(dm-plot libdm libbu ${TCL_LIBRARY})
  set_property(TARGET dm-plot APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-plot "${PLOT_SRCS}")

  PLUGIN_SETUP(dm-plot dm)

endif(BRLCAD_ENABLE_TCL)

CMAKEFILES(
  ${PLOT_SRCS}
  CMakeLists.txt
  dm-plot.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
