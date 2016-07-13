set(ADRT_INCLUDE_DIRS
  ${GCV_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES ADRT_INCLUDE_DIRS)
include_directories(${ADRT_INCLUDE_DIRS})

include_directories(
  ${BRLCAD_SOURCE_DIR}/src/adrt
  ${BRLCAD_SOURCE_DIR}/src/adrt/libcommon
  ${BRLCAD_SOURCE_DIR}/src/adrt/librender
  ${TINYCTHREAD_INCLUDE_DIR}
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
BRLCAD_ADDLIB(librender "${LIBRENDER_SRCS}" "libgcv;${TINYCTHREAD_LIBRARY}")
set_target_properties(librender PROPERTIES VERSION 20.0.1 SOVERSION 20)
add_dependencies(librender libgcv_plugins)

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

if(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

  set(isst_dirs
    ${TCLCAD_INCLUDE_DIRS}
    ${DM_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR_GL}
    ${OPENGL_INCLUDE_DIR_GLX}
    )
  BRLCAD_INCLUDE_DIRS(isst_dirs)

  set(isst_libs libtclcad librender libdm ${TCL_LIBRARIES} ${OPENGL_LIBRARIES} ${TINYCTHREAD_LIBRARY})

  if(CPP_DLL_DEFINES)
    set(isst_libs ${isst_libs} imm32.lib comctl32.lib opengl32.lib)
  endif(CPP_DLL_DEFINES)

  BRLCAD_ADDDATA(isst.tcl tclscripts/isst)
  BRLCAD_ADDEXEC(isst isst.c "${isst_libs}" GUI)
  add_dependencies(isst isst_tcl_cp)

  foreach(item ${tclindex_target_list})
    add_dependencies(isst ${item})
  endforeach(item ${tclindex_target_list})

endif(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

CMAKEFILES(isst.c)


add_subdirectory(master)
# add_subdirectory(slave)

set(adrt_ignore_files
  load.h
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
