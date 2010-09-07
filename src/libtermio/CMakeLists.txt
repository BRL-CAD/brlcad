include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_TERMLIB_INCLUDE_DIR}
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
)

add_library(termio termio.c)
target_link_libraries(termio ${BRLCAD_TERMLIB_LIBRARY})
INSTALL(TARGETS termio DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
