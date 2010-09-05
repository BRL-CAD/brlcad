include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_STD_INCLUDES}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

BRLCAD_ADDEXEC(vdeck "cgarbs.c match.c parsarg.c vdeck.c" librt)
