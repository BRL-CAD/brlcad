include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  )

set(PLOT_SRCS
  dm-plot.c
  )

add_definitions(-DDM_PLUGIN)

add_library(dm-plot SHARED ${PLOT_SRCS})
target_link_libraries(dm-plot libdm libbu)
if (CPP_DLL_DEFINES)
  set_property(TARGET dm-plot APPEND PROPERTY COMPILE_DEFINITIONS "DM_DLL_EXPORTS")
endif (CPP_DLL_DEFINES)
VALIDATE_STYLE(dm-plot "${PLOT_SRCS}")

PLUGIN_SETUP(dm-plot dm)

CMAKEFILES(dm-plot.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
