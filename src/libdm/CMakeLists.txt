# Include directories needed by libdm users
set(DM_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  )

# local includes
set(DM_LOCAL_INCLUDE_DIRS ${PNG_INCLUDE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(dm DM_INCLUDE_DIRS DM_LOCAL_INCLUDE_DIRS)

get_property(DM_DEFINES GLOBAL PROPERTY DM_DEFINES)
# Add the individual OS specific files and definitions
if(WIN32 AND BRLCAD_ENABLE_OPENGL)
  list(APPEND DM_DEFINES DM_WGL IF_WGL)
  set(dmw32_srcs dm-wgl.c)
endif(WIN32 AND BRLCAD_ENABLE_OPENGL)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)
  list(APPEND DM_DEFINES DM_X IF_X)
  set(dmx11_srcs dm-X.c)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
  list(APPEND DM_DEFINES DM_OGL IF_OGL)
  set(dmogl_srcs dm-ogl.c)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_RTGL)
  list(APPEND DM_DEFINES DM_RTGL IF_RTGL)
  set(dmrtgl_srcs dm-rtgl.c)
endif(BRLCAD_ENABLE_RTGL)

if(BRLCAD_ENABLE_TK AND NOT WIN32)
  list(APPEND DM_DEFINES DM_TK IF_TK)
  set(DM_TKLIB ${TCL_TK_LIBRARY})
  set(dmtk_srcs dm-tk.c)
endif(BRLCAD_ENABLE_TK AND NOT WIN32)

set_property(GLOBAL PROPERTY DM_DEFINES "${DM_DEFINES}")

# Unfortunately, on Mac OS X the OpenGL headers are not C90
# compilant - anything pulling them in needs the C99 flag
get_property(DM_C_FLAGS GLOBAL PROPERTY DM_C_FLAGS)
list(APPEND DM_C_DEFINES "${C99_FLAG}")
set_property(GLOBAL PROPERTY DM_C_FLAGS "${DM_C_FLAGS}")

set(LIBDM_SRCS
  ${dmx11_srcs}
  ${dmw32_srcs}
  ${dmogl_srcs}
  ${dmrtgl_srcs}
  ${dmtk_srcs}
  adc.c
  axes.c
  clip.c
  color.c
  dm-Null.c
  dm-generic.c
  dm-plot.c
  dm-ps.c
  dm_obj.c
  focus.c
  grid.c
  knob.c
  labels.c
  options.c
  query.c
  rect.c
  scale.c
  tcl.c
  vers.c
  )

BRLCAD_ADDLIB(libdm "${LIBDM_SRCS}" "libged;librt;libfb;${X11_LIBRARIES};${X11_Xi_LIB};${DM_TKLIB};${Carbon_LIBRARIES}")
set_target_properties(libdm PROPERTIES VERSION 19.0.1 SOVERSION 19)

if(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
  add_dependencies(libdm tk)
endif(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
set(libdm_ignore_files
  dm-wgl.c
  dm-X.c
  dm-ogl.c
  dm-rtgl.c
  dm-tk.c
  TODO
  )
CMAKEFILES(${libdm_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
