# Something in the standard build flags doesn't interact well with
# the lex output here, and even -w doesn't avoid the issue (??)
# so for this one directory, go vanilla until we can get a lex/yacc
# setup that reliably produces code that will work in strict
# environments
set(CMAKE_C_FLAGS "")

set(MGED_INCLUDE_DIRS ${MGED_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(MGED_INCLUDE_DIRS)

YACC_TARGET(MGEDPointsParser points_parse.y
  ${CMAKE_CURRENT_BINARY_DIR}/points_parse.c)

if(ENABLE_POSIX_COMPLIANCE)
  # need something special here for fileno:
  LEX_TARGET(MGEDPointsScanner points_scan.l
    ${CMAKE_CURRENT_BINARY_DIR}/points_scan.c COMPILE_FLAGS "-l -D_POSIX_C_SOURCE=200112L")
else()
  LEX_TARGET(MGEDPointsScanner points_scan.l
    ${CMAKE_CURRENT_BINARY_DIR}/points_scan.c COMPILE_FLAGS "-l")
endif()

ADD_LEX_YACC_DEPENDENCY(MGEDPointsScanner MGEDPointsParser)

set(MGED_POINTS_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/points_parse.c
  ${CMAKE_CURRENT_BINARY_DIR}/points_scan.c
  count.c
  process.c
  main.c
  )

BRLCAD_ADDLIB(libpoints "${MGED_POINTS_SRCS}" libbu)
set_target_properties(libpoints PROPERTIES VERSION 20.0.1 SOVERSION 20)
set_target_properties(libpoints PROPERTIES COMPILE_DEFINITIONS "ENABLE_POINTS")
set(mged_points_ignore_files
  count.h
  points_parse.y
  points_scan.l
  process.h
  )
CMAKEFILES(${mged_points_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
