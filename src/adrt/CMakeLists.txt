set(ADRT_INCLUDE_DIRS
  ${GCV_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES ADRT_INCLUDE_DIRS)
include_directories(${ADRT_INCLUDE_DIRS})

include_directories(
  ${BRLCAD_SOURCE_DIR}/src/adrt
  ${BRLCAD_SOURCE_DIR}/src/adrt/libcommon
  ${BRLCAD_SOURCE_DIR}/src/adrt/librender
  )

set(LIBRENDER_SRCS
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
BRLCAD_ADDLIB(librender "${LIBRENDER_SRCS}" libgcv)

set(tie_HDRS
  adrt.h
  adrt_struct.h
  librender/camera.h
  librender/render.h
  librender/render_internal.h
  librender/render_util.h
  librender/texture.h
  librender/texture_internal.h
  )
install(FILES ${tie_HDRS} DESTINATION include/tie)
CMAKEFILES(${tie_HDRS})

# Tcl/Tk based ISST
INCLUDE_DIRECTORIES(
  ${TOGL_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${X11_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/../other/togl/src
  )
if(TOGL_LIBRARIES AND BRLCAD_ENABLE_OPENGL)
  ADD_DEFINITIONS(-DTOGL_USE_EXTERNAL_CONFIG_H=1)
  add_library(issttcltk isst_tcltk.c)
  if(WIN32)
    target_link_libraries(issttcltk librender ${TCL_LIBRARIES} ${TOGL_STUB_LIBRARIES} ${OPENGL_LIBRARIES} opengl32.lib)
  else(WIN32)
    target_link_libraries(issttcltk librender ${TCL_LIBRARIES} ${TOGL_LIBRARIES})
  endif(WIN32)
  install(TARGETS issttcltk 
    RUNTIME DESTINATION ${BIN_DIR}/isst0.1
    LIBRARY DESTINATION ${LIB_DIR}/isst0.1
    ARCHIVE DESTINATION ${LIB_DIR}/isst0.1)
  include(${BRLCAD_SOURCE_DIR}/misc/CMake/TCL_PKGINDEX.cmake)
  TCL_PKGINDEX(issttcltk isst "0.1")
  # TODO - need to make this configure line into a real target ala
  # BRLCAD_ADDDATA in order for the build dir copy to be updated when it
  # is changed.
  if(WIN32)
    configure_file(isst.bat ${CMAKE_BINARY_DIR}/bin/isst.bat COPYONLY)
    install(PROGRAMS isst.bat DESTINATION bin)
  endif(WIN32)
  configure_file(isst ${CMAKE_BINARY_DIR}/bin/isst COPYONLY)
  install(PROGRAMS isst DESTINATION bin)
endif(TOGL_LIBRARIES AND BRLCAD_ENABLE_OPENGL)

set(adrt_ignore_files
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
