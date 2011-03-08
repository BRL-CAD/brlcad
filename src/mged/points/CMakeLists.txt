# Something in the standard build flags doesn't interact well with
# the lex output here, and even -w doesn't avoid the issue (??) 
# so for this one directory, go vanilla until we can get a lex/yacc 
# setup that reliably produces code that will work in strict 
# environments
STRING(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
IF(BUILD_TYPE)
	SET(CMAKE_C_FLAGS_${BUILD_TYPE} "")
ELSE(BUILD_TYPE)
	SET(CMAKE_C_FLAGS "")
ENDIF(BUILD_TYPE)

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
INSTALL(TARGETS libpoints DESTINATION lib)
SET_TARGET_PROPERTIES(libpoints PROPERTIES COMPILE_DEFINITIONS "ENABLE_POINTS")
SET(mged_points_ignore_files
	count.h
	points_parse.y
	points_scan.l
	process.h
	)
CMAKEFILES(${mged_points_ignore_files})
