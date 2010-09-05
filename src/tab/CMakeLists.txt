include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${BRLCAD_STD_INCLUDES}
)
add_definitions(
  ${BRLCAD_STD_DEFS}
)

LEX_TARGET(SS_Scanner script.l
    ${CMAKE_CURRENT_BINARY_DIR}/script.c COMPILE_FLAGS "-l")

BRLCAD_ADDEXEC(scriptsort "scriptsort.c ${LEX_SS_Scanner_OUTPUTS}" librt)
