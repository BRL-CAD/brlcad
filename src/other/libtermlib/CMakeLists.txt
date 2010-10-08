
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/termcap.src
    ${CMAKE_CURRENT_BINARY_DIR}/termcap COPYONLY)


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
	-DB_TERMCAP=\"${CMAKE_INSTALL_PREFIX}/etc/termcap\"
)

add_library(termlib SHARED ${LIBTERM_SOURCES})
INSTALL(TARGETS termlib DESTINATION lib)

add_library(termlib-static STATIC ${LIBTERM_SOURCES})
INSTALL(TARGETS termlib-static ARCHIVE DESTINATION lib)
SET_TARGET_PROPERTIES(termlib-static PROPERTIES OUTPUT_NAME "termlib")

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/termcap DESTINATION share/etc)

