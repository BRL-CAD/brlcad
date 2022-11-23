set(X_SRCS
  dm-X.c
  color.c
  if_X24.c
  )

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)

  find_package(X11)
  find_package(TCL)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${X11_INCLUDE_DIR}
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
    )

  set_property(SOURCE dm-X.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE if_X24.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  add_definitions(-DDM_PLUGIN)

  if (X11_Xi_LIB)
    list(APPEND X11_LIBRARIES ${X11_Xi_LIB})
  endif (X11_Xi_LIB)

  dm_plugin_library(dm-X SHARED ${X_SRCS})
  target_link_libraries(dm-X libdm libbu ${TCL_LIBRARY} ${TK_LIBRARY} ${X11_LIBRARIES})
  set_property(TARGET dm-X APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-X "${X_SRCS}")

  PLUGIN_SETUP(dm-X dm)

endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)

CMAKEFILES(
  CMakeLists.txt
  ${X_SRCS}
  dm-X.h
  fb_X.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
