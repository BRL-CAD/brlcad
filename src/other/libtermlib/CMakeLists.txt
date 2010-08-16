
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

add_library(termlib ${LIBTERM_SOURCES})

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/termcap DESTINATION etc)

