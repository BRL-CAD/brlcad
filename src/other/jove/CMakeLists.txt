include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_BINARY_DIR}/include
	${CMAKE_SOURCE_DIR}/include
	${TERMLIB_INCLUDE_DIR}
	)

add_definitions(
	-DHAVE_CONFIG_H=1
	-DBRLCADBUILD=1
	)

SET(JOVE_SRCS
	jove_buf.c
	jove_c.c
	jove_delete.c
	jove_disp.c
	jove_draw.c
	jove_extend.c
	jove_funcs.c
	jove_insert.c
	jove_io.c
	jove_main.c
	jove_marks.c
	jove_misc.c
	jove_proc.c
	jove_re.c
	jove_screen.c
	jove_term.c
	jove_tune.c
	jove_wind.c
	)

add_executable(jove ${JOVE_SRCS})
target_link_libraries(jove ${TERMLIB_LIBRARY})
INSTALL(TARGETS jove DESTINATION bin)

SET(findcom_SOURCES
	findcom.c
	jove_tune.c
	)
add_executable(findcom ${findcom_SOURCES})
INSTALL(TARGETS findcom DESTINATION bin)

FILE(COPY jove.1 DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man1)
INSTALL(FILES jove.1 DESTINATION	${MAN_DIR}/man1)

SET(jove_DATA
	describe.com
	jove.emacs
	jove-tutorial
	)

FILE(COPY ${jove_DATA} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/jove)
INSTALL(FILES ${jove_DATA} DESTINATION	${DATA_DIR}/jove)

