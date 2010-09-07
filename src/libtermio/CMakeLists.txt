include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_TERMLIB_INCLUDE_DIR}
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
)

BRLCAD_ADDLIB(termio termio.c "${BRLCAD_TERMLIB_LIBRARY}")
