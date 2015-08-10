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
set_target_properties(librender PROPERTIES VERSION 20.0.1 SOVERSION 20)

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
set(isst_dirs
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${X11_INCLUDE_DIR}
  )
BRLCAD_INCLUDE_DIRS(isst_dirs)
if(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
  if(WIN32)
    BRLCAD_ADDLIB(issttcltk isst_tcltk.c "librender;libdm;${TCL_LIBRARIES};${OPENGL_LIBRARIES};opengl32.lib" NO_INSTALL)
  else(WIN32)
    BRLCAD_ADDLIB(issttcltk isst_tcltk.c "librender;libdm;${TCL_LIBRARIES};" NO_INSTALL)
  endif(WIN32)
  install(TARGETS issttcltk
    RUNTIME DESTINATION ${BIN_DIR}/isst0.1
    LIBRARY DESTINATION ${LIB_DIR}/
    ARCHIVE DESTINATION ${LIB_DIR}/isst0.1)
  include(${BRLCAD_SOURCE_DIR}/misc/CMake/TCL_PKGINDEX.cmake)
  TCL_PKGINDEX(issttcltk isst "0.1")
  if(WIN32)
    BRLCAD_MANAGE_FILES("isst.bat" ${BIN_DIR} EXEC)
  endif(WIN32)
  BRLCAD_MANAGE_FILES("isst" ${BIN_DIR} EXEC)
  add_custom_target(isst ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/isst)
  set_target_properties(isst PROPERTIES FOLDER "BRL-CAD Executables")
  add_dependencies(isst issttcltk bwish)
  foreach(item ${tclindex_target_list})
    add_dependencies(isst ${item})
  endforeach(item ${tclindex_target_list})
endif(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

CMAKEFILES(isst_tcltk.c)

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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
