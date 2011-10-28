IF(NOT MSVC)

SET(LIBTERMLIB_PRIVATE_HDRS
	termcap.h
)


SET(LIBTERM_SOURCES
	termcap.c
	tgoto.c
	tputs.c
)

add_definitions(
   -DCM_N
	-DCM_GT
	-DCM_B
	-DCM_D
	-DB_TERMCAP=\"${CMAKE_INSTALL_PREFIX}/${DATA_DIR}/etc/termcap\"
)

add_library(termlib SHARED ${LIBTERM_SOURCES})
INSTALL(TARGETS termlib DESTINATION ${LIB_DIR})

add_library(termlib-static STATIC ${LIBTERM_SOURCES})
INSTALL(TARGETS termlib-static ARCHIVE DESTINATION ${LIB_DIR})
SET_TARGET_PROPERTIES(termlib-static PROPERTIES OUTPUT_NAME "termlib")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/termcap.src ${CMAKE_BINARY_DIR}/${DATA_DIR}/etc/termcap COPYONLY)
INSTALL(FILES ${CMAKE_BINARY_DIR}/${DATA_DIR}/etc/termcap DESTINATION ${DATA_DIR}/etc)

ENDIF(NOT MSVC)
