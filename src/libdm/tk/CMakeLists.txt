set(TK_SRCS
  dm-tk.c
  if_tk.c
  )

if(BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_DM_TK)

  find_package(TCL)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
    )

  set_property(SOURCE dm-tk.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE if_tk24.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  add_definitions(-DDM_PLUGIN)

  dm_plugin_library(dm-tk SHARED ${TK_SRCS})
  target_link_libraries(dm-tk libdm libbu ${TCL_LIBRARY} ${TK_LIBRARY})
  set_property(TARGET dm-tk APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-tk "${TK_SRCS}")

  PLUGIN_SETUP(dm-tk dm)

endif(BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_DM_TK)

CMAKEFILES(
  CMakeLists.txt
  ${TK_SRCS}
  dm-tk.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
