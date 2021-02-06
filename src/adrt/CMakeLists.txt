set(ADRT_INCLUDE_DIRS
  ${GCV_INCLUDE_DIRS}
  ${BRLCAD_SOURCE_DIR}/src/adrt
  ${BRLCAD_SOURCE_DIR}/src/adrt/libcommon
  ${BRLCAD_SOURCE_DIR}/src/adrt/librender
  )
list(REMOVE_DUPLICATES ADRT_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(ADRT_INCLUDE_DIRS)

if(NOT "${Z_PREFIX_STR}" STREQUAL "")
  add_definitions(-DZ_PREFIX)
  add_definitions(-DZ_PREFIX_STR=${Z_PREFIX_STR})
endif(NOT "${Z_PREFIX_STR}" STREQUAL "")

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
BRLCAD_ADDLIB(librender "${LIBRENDER_SRCS}" "libgcv")
set_target_properties(librender PROPERTIES VERSION 20.0.1 SOVERSION 20)
add_dependencies(librender gcv_plugins)

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
# TODO - should these be in brlcad/tie instead?
BRLCAD_MANAGE_FILES(tie_HDRS ${INCLUDE_DIR}/tie)

# Tcl/Tk based ISST

if(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

  set(isst_dirs
    ${TCLCAD_INCLUDE_DIRS}
    ${DM_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR_GL}
    ${OPENGL_INCLUDE_DIR_GLX}
    )
  BRLCAD_INCLUDE_DIRS(isst_dirs)

  set(isst_libs libtclcad librender libdm ${TCL_LIBRARY} ${OPENGL_LIBRARIES} ${IMM32_LIBRARY} ${COMCTL32_LIBRARY} ${OPENGL32_LIBRARY})

  BRLCAD_ADDDATA(isst.tcl tclscripts/isst)
  BRLCAD_ADDEXEC(isst isst.c "${isst_libs}" GUI)
  ADD_TARGET_DEPS(isst isst_tcl_cp dm_plugins)
  foreach(item ${tclindex_target_list})
    add_dependencies(isst ${item})
  endforeach(item ${tclindex_target_list})

endif(BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

CMAKEFILES(isst.tcl)
CMAKEFILES(isst.c)

add_subdirectory(master)
# add_subdirectory(slave)

set(adrt_ignore_files
  load.h
  slave/slave.c
  slave/tienet_slave.h
  slave/tienet_slave.c
  slave/slave.h
  tienet.h
  )
CMAKEFILES(${adrt_ignore_files})
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
