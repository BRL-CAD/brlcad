set(PS_SRCS
  dm-ps.c
  )

if(BRLCAD_ENABLE_TCL)

  find_package(TCL)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${TCL_INCLUDE_PATH}
    )

  add_definitions(-DDM_PLUGIN)

  dm_plugin_library(dm-ps SHARED ${PS_SRCS})
  target_link_libraries(dm-ps libdm libbu ${TCL_LIBRARY})
  set_property(TARGET dm-ps APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-ps "${PS_SRCS}")

  PLUGIN_SETUP(dm-ps dm)

endif(BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${PS_SRCS}
  dm-ps.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
