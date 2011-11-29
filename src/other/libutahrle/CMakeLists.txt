SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)
INCLUDE(UTAH_CheckFunctions)

IF(NOT WIN32)
	UTAH_CHECK_LIBRARY(M m cos)
ENDIF(NOT WIN32)

include_directories(
	${${CMAKE_PROJECT_NAME}_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_BINARY_DIR}
)

SET(LIBUTAHRLE_PUBLIC_HDRS
	${CMAKE_CURRENT_SOURCE_DIR}/include/exit_status.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/pyramid.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rle.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rle_code.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rle_config.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rle_put.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rle_raw.h
)

SET(LIBUTAHRLE_PRIVATE_HDRS
	${CMAKE_CURRENT_SOURCE_DIR}/include/colorquant.h
)

SET(LIBUTAHRLE_SOURCES
	Runput.c
	buildmap.c
	cmd_name.c
	colorquant.c
	dither.c
	float_to_exp.c
	hilbert.c
	inv_cmap.c
	rle_addhist.c
	rle_cp.c
	rle_error.c
	rle_getcom.c
	rle_getraw.c
	rle_getrow.c
	rle_getskip.c
	rle_global.c
	rle_hdr.c
	rle_open_f.c
	rle_putcom.c
	rle_putraw.c
	rle_putrow.c
	rle_raw_alc.c
	rle_rawrow.c
	rle_row_alc.c
	scanargs.c
	vaxshort.c
)

IF(MSVC)
	add_definitions(
		-DRLE_DLL_EXPORTS
		-DNO_DECLARE_MALLOC
		-DNEED_BSTRING
		-DNO_OPEN_PIPES
		)
ENDIF(MSVC)

add_library(utahrle SHARED ${LIBUTAHRLE_SOURCES})
target_link_libraries(utahrle ${M_LIBRARY})
SET_TARGET_PROPERTIES(utahrle PROPERTIES VERSION 19.0.1 SOVERSION 19)
INSTALL(TARGETS utahrle DESTINATION ${LIB_DIR})
add_library(utahrle-static STATIC ${LIBUTAHRLE_SOURCES})
target_link_libraries(utahrle-static ${M_LIBRARY})
IF(NOT WIN32)
	SET_TARGET_PROPERTIES(utahrle-static PROPERTIES OUTPUT_NAME "utahrle")
ENDIF(NOT WIN32)
INSTALL(TARGETS utahrle-static ARCHIVE DESTINATION ${LIB_DIR})

SET(LIBUTAHRLE_MAN3
	buildmap.3
	bwdithermap.3
	colorquant.3
	dither.3
	ditherbw.3
	dithergb.3
	dithermap.3
	float_to_exp.3
	hilbert.3
	hilbert_c2i.3
	hilbert_i2c.3
	inv_cmap.3
	librle.3
	libutahrle.3
	make_square.3
	rgb_to_bw.3
	rle_addhist.3
	rle_cp.3
	rle_debug.3
	rle_delcom.3
	rle_freeraw.3
	rle_get_error.3
	rle_get_set.3
	rle_get_setup.3
	rle_get_setup_ok.3
	rle_getcom.3
	rle_getraw.3
	rle_getrow.3
	rle_getskip.3
	rle_hdr.3
	rle_op.3
	rle_open_f.3
	rle_open_f_noexit.3
	rle_put_init.3
	rle_put_setup.3
	rle_putcom.3
	rle_puteof.3
	rle_putraw.3
	rle_putrow.3
	rle_raw_alloc.3
	rle_raw_free.3
	rle_rawtorow.3
	rle_row_alloc.3
	rle_row_free.3
	rle_skiprow.3
	scanargs.3
)
IF(NOT MAN_DIR)
  SET(MAN_DIR "share/man")
ENDIF(NOT MAN_DIR)

if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL )
    install(FILES ${LIBUTAHRLE_MAN3} DESTINATION ${MAN_DIR}/man3)
    install(FILES rle.5 DESTINATION ${MAN_DIR}/man5)
	 INSTALL(FILES ${LIBUTAHRLE_PUBLIC_HDRS} DESTINATION include)
endif()
