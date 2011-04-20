include_directories(
  ${TCL_INCLUDE_DIRS}
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
	INSTALL(TARGETS librender DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_LIB_DIR})
	IF(WIN32)
		SET_TARGET_PROPERTIES(librender PROPERTIES COMPILE_FLAGS "-DRENDER_EXPORT_DLL")
	ENDIF(WIN32)
ENDIF(BUILD_SHARED_LIBS)
IF(BUILD_STATIC_LIBS AND NOT MSVC)
	add_library(librender-static STATIC ${LIBRENDER_SRCS})
	target_link_libraries(librender-static libgcv)
	IF(NOT WIN32)
		SET_TARGET_PROPERTIES(librender-static PROPERTIES OUTPUT_NAME "librender")
	ENDIF(NOT WIN32)
	IF(WIN32)
	   SET_TARGET_PROPERTIES(librender-static PROPERTIES COMPILE_FLAGS "-DRENDER_EXPORT_DLL")
		SET_TARGET_PROPERTIES(librender-static PROPERTIES PREFIX "lib")
	ENDIF(WIN32)
	INSTALL(TARGETS librender-static DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_LIB_DIR})
ENDIF(BUILD_STATIC_LIBS AND NOT MSVC)

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
SET(adrt_ignore_files
	isst.h
	isst.tcl
	isst_tcltk.c
	load.h
	master
	slave
	tienet.h
	)
CMAKEFILES(${adrt_ignore_files})
