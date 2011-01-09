include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_DIRS}
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

add_library(libpoints ${MGED_POINTS_SRCS})
target_link_libraries(libpoints libbu)
INSTALL(TARGETS libpoints DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
SET_TARGET_PROPERTIES(libpoints PROPERTIES COMPILE_DEFINITIONS "ENABLE_POINTS")
