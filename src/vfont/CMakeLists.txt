include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_STD_INCLUDES}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

add_library(vfont getfont.c)
target_link_libraries(vfont bu)

#INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/fix.6r DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/vfont)
#INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/nonie.r.12 DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/vfont)
