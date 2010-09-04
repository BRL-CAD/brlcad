include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${TCL_INCLUDE_PATH}
  ${BRLCAD_SOURCE_DIR}/src/adrt
  ${BRLCAD_SOURCE_DIR}/src/adrt/libtie
  ${BRLCAD_SOURCE_DIR}/src/adrt/libcommon
  ${BRLCAD_SOURCE_DIR}/src/adrt/librender
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
)


SET(LIBTIE_PRECISION_SRCS "libtie/tie.c;libtie/tie_kdtree.c")
add_library(tie0 ${LIBTIE_PRECISION_SRCS})
target_link_libraries(tie0 bu)
SET_TARGET_PROPERTIES(tie0 PROPERTIES COMPILE_DEFINITIONS "TIE_PRECISION=0")
add_library(tie ${LIBTIE_PRECISION_SRCS})
target_link_libraries(tie tie0 bu)
INSTALL(TARGETS tie LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
SET_TARGET_PROPERTIES(tie PROPERTIES COMPILE_DEFINITIONS "TIE_PRECISION=1")

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
add_library(render ${LIBRENDER_SRCS})
target_link_libraries(render tie gcv) 
INSTALL(TARGETS render LIBRARY DESTINATION ${BRLCAD_INSTALL_LIB_DIR})

SET(ADRT_MASTER_SRCS
  master/compnet.c 
  master/dispatcher.c 
  master/tienet_master.c 
  master/master.c
)
add_executable(adrt_master ${ADRT_MASTER_SRCS})
target_link_libraries(adrt_master tie bu ${M_LIBRARY} ${ZLIB_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIBRARY})
INSTALL(TARGETS adrt_master RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})

SET(ADRT_SLAVE_SRCS
  slave/slave.c
  slave/tienet_slave.c
)
add_executable(adrt_slave ${ADRT_MASTER_SRCS})
target_link_libraries(adrt_slave tie render ${CMAKE_THREAD_LIBS_INIT})
INSTALL(TARGETS adrt_slave RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})
