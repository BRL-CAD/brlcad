SET(ADRT_INCLUDE_DIRS
	${GCV_INCLUDE_DIRS}
	)
LIST(REMOVE_DUPLICATES ADRT_INCLUDE_DIRS)
include_directories(${ADRT_INCLUDE_DIRS})

include_directories(
  ${BRLCAD_SOURCE_DIR}/src/adrt
  ${BRLCAD_SOURCE_DIR}/src/adrt/libcommon
  ${BRLCAD_SOURCE_DIR}/src/adrt/librender
)

SET(LIBRENDER_SRCS
  load.c
  load_g.c
  librender/camera.c
  librender/component.c
  librender/cut.c
  librender/depth.c
  librender/flat.c
  librender/flos.c
  librender/grid.c
  librender/hit.c
  librender/normal.c
  librender/path.c
  librender/phong.c
  librender/render_util.c
  librender/spall.c
  librender/surfel.c
  librender/texture_blend.c
  librender/texture_bump.c
  librender/texture_camo.c
  librender/texture_checker.c
  librender/texture_clouds.c
  librender/texture_gradient.c
  librender/texture_image.c
  librender/texture_mix.c
  librender/texture_perlin.c
  librender/texture_stack.c
)
IF(BUILD_SHARED_LIBS)
	add_library(librender SHARED ${LIBRENDER_SRCS})
	target_link_libraries(librender libgcv)
	INSTALL(TARGETS librender DESTINATION ${LIB_DIR})
	IF(WIN32)
		SET_TARGET_PROPERTIES(librender PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL -DRENDER_EXPORT_DLL")
	ENDIF(WIN32)
ENDIF(BUILD_SHARED_LIBS)
IF(BUILD_STATIC_LIBS)
	add_library(librender-static STATIC ${LIBRENDER_SRCS})
	target_link_libraries(librender-static libgcv)
	IF(NOT WIN32)
		SET_TARGET_PROPERTIES(librender-static PROPERTIES OUTPUT_NAME "librender")
	ENDIF(NOT WIN32)
	INSTALL(TARGETS librender-static DESTINATION ${LIB_DIR})
ENDIF(BUILD_STATIC_LIBS)

SET(tie_HDRS
	adrt.h
	adrt_struct.h
	librender/camera.h
	librender/render.h
	librender/render_internal.h
	librender/render_util.h
	librender/texture.h
	librender/texture_internal.h
)
INSTALL(FILES ${tie_HDRS} DESTINATION include/tie)
CMAKEFILES(${tie_HDRS})

# Tcl/Tk based ISST
INCLUDE_DIRECTORIES(
	${TOGL_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIR_GL}
	${OPENGL_INCLUDE_DIR_GLX}
	${X11_INCLUDE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/../other/togl/src
	)
ADD_DEFINITIONS(
	-DTOGL_USE_EXTERNAL_CONFIG_H=1
	)
IF(TOGL_LIBRARIES)
	add_library(issttcltk isst_tcltk.c)
	IF(WIN32)
		target_link_libraries(issttcltk librender ${TCL_LIBRARIES} ${TOGL_STUB_LIBRARIES} ${OPENGL_LIBRARIES})
	ELSE(WIN32)
		target_link_libraries(issttcltk librender ${TCL_LIBRARIES} ${TOGL_LIBRARIES})
	ENDIF(WIN32)
	INSTALL(TARGETS issttcltk DESTINATION ${LIB_DIR}/isst0.1)
	GET_TARGET_PROPERTY(ISSTTCL_LIBLOCATION issttcltk LOCATION_${CMAKE_BUILD_TYPE})
	GET_FILENAME_COMPONENT(ISSTTCL_LIBNAME ${ISSTTCL_LIBLOCATION} NAME)
	FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded isst	0.1 [list load [file join $dir .. .. ${LIB_DIR} isst0.1 ${ISSTTCL_LIBNAME}]]")
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/isst0.1)
	FILE(WRITE ${CMAKE_BINARY_DIR}/lib/isst0.1/pkgIndex.tcl "package ifneeded isst 0.1 [list load [file join $dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}	${ISSTTCL_LIBNAME}] isst]")
	# TODO - need to make this configure line into a real target ala
	# BRLCAD_ADDDATA in order for the build dir copy to be updated when it
   # is changed.
	IF(WIN32)
		configure_file(isst.bat ${CMAKE_BINARY_DIR}/bin/isst.bat COPYONLY)
		INSTALL(PROGRAMS isst.bat DESTINATION bin)
	ENDIF(WIN32)
	configure_file(isst ${CMAKE_BINARY_DIR}/bin/isst COPYONLY)
	install(PROGRAMS isst DESTINATION bin)

ENDIF(TOGL_LIBRARIES)

SET(adrt_ignore_files
	isst.h
	isst
	isst.bat
	load.h
	master
	slave
	tienet.h
	)
CMAKEFILES(${adrt_ignore_files})
CMAKEFILES(Makefile.am)
