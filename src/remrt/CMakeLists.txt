include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_STD_INCLUDES}
  ${TCL_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)

BRLCAD_ADDEXEC(remrt "ihost.c remrt.c" "libremrt optical fb")
BRLCAD_ADDEXEC(rtsrv rtsrv.c "libremrt optical pkg")
SET_TARGET_PROPERTIES(rtsrv PROPERTIES COMPILE_DEFINITIONS "RTSRV")
