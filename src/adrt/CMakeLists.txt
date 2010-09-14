include_directories(
  ${TCL_INCLUDE_PATH}
  ${BRLCAD_SOURCE_DIR}/src/adrt
  ${BRLCAD_SOURCE_DIR}/src/adrt/libtie
  ${BRLCAD_SOURCE_DIR}/src/adrt/libcommon
  ${BRLCAD_SOURCE_DIR}/src/adrt/librender
)

SET(LIBTIE_PRECISION_SRCS "libtie/tie.c;libtie/tie_kdtree.c")
add_library(libtie0 ${LIBTIE_PRECISION_SRCS})
target_link_libraries(libtie0 libbu)
SET_TARGET_PROPERTIES(libtie0 PROPERTIES COMPILE_DEFINITIONS "TIE_PRECISION=0")
BRLCAD_ADDLIB(libtie "${LIBTIE_PRECISION_SRCS}" "libtie0 libbu")
SET_TARGET_PROPERTIES(libtie PROPERTIES COMPILE_DEFINITIONS "TIE_PRECISION=1")

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
BRLCAD_ADDLIB(librender "${LIBRENDER_SRCS}" "libtie libgcv")

SET(ADRT_MASTER_SRCS
  master/compnet.c 
  master/dispatcher.c 
  master/tienet_master.c 
  master/master.c
)
BRLCAD_ADDEXEC(adrt_master "${ADRT_MASTER_SRCS}" "libtie libbu ${M_LIBRARY} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIBRARY}")

SET(ADRT_SLAVE_SRCS
  slave/slave.c
  slave/tienet_slave.c
)
BRLCAD_ADDEXEC(adrt_slave "${ADRT_MASTER_SRCS}" "libtie librender ${CMAKE_THREAD_LIBS_INIT}")

SET(tie_HDRS
	adrt.h  
	adrt_struct.h 
	librender/camera.h 
	librender/render.h 
	librender/render_internal.h 
	librender/render_util.h 
	librender/texture.h 
	librender/texture_internal.h 
	libtie/tie.h
)
INSTALL(FILES ${tie_HDRS} DESTINATION include/tie)
