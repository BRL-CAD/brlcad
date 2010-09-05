include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${BRLCAD_STD_INCLUDES}
)
add_definitions(
  ${BRLCAD_STD_DEFS}
)

YACC_TARGET(MGEDPointsParser points_parse.y
  ${CMAKE_CURRENT_BINARY_DIR}/points_parse.c)
LEX_TARGET(MGEDPointsScanner points_scan.l
  ${CMAKE_CURRENT_BINARY_DIR}/points_scan.c COMPILE_FLAGS "-l")
ADD_LEX_YACC_DEPENDENCY(MGEDPointsScanner MGEDPointsParser)

SET(MGED_POINTS_SRCS
  ${YACC_MGEDPointsParser_OUTPUTS}
  ${LEX_MGEDPointsScanner_OUTPUTS}
  count.c
  process.c
  main.c
)

BRLCAD_ADDLIB(points "${MGED_POINTS_SRCS}" bu)
SET_TARGET_PROPERTIES(points PROPERTIES COMPILE_DEFINITIONS "ENABLE_POINTS")
