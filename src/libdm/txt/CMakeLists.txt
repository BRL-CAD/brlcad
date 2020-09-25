set(TXT_SRCS
  dm-txt.c
  if_debug.c
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

  dm_plugin_library(dm-txt SHARED ${TXT_SRCS})
  target_link_libraries(dm-txt libdm libbu ${TCL_LIBRARY})
  set_property(TARGET dm-txt APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-txt "${TXT_SRCS}")

  PLUGIN_SETUP(dm-txt dm)

endif(BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${TXT_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
