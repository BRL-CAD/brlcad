# Include directories needed by libdm users
set(DM_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
  ${OPENNURBS_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${OPENTHREADS_INCLUDE_DIR}
  ${OSG_INCLUDE_DIR}
  )

# local includes
set(DM_LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/null ${PNG_INCLUDE_DIRS})

BRLCAD_LIB_INCLUDE_DIRS(dm DM_INCLUDE_DIRS DM_LOCAL_INCLUDE_DIRS)

# Initialize libdm_DEFINES in case of reconfiguration
set(libdm_DEFINES "")

# Set libdm compilation definitions
get_property(libdm_DEFINES GLOBAL PROPERTY libdm_DEFINES)
# Add the individual OS specific files and definitions
if(WIN32 AND BRLCAD_ENABLE_OPENGL)
  list(APPEND libdm_DEFINES DM_WGL IF_WGL)
  set(dmw32_srcs wgl/dm-wgl.c wgl/if_wgl.c)
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${OPENGL_LIBRARIES})
  set_property(SOURCE wgl/dm-wgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE wgl/if_wgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(WIN32 AND BRLCAD_ENABLE_OPENGL)

if(BRLCAD_ENABLE_X11)
  set(DM_X_LIBS ${X11_LIBRARIES})
endif(BRLCAD_ENABLE_X11)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)
  list(APPEND libdm_DEFINES DM_X IF_X)
  set(dmx11_srcs X/dm-X.c X/if_X24.c)
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${X11_LIBRARIES})
  set_property(SOURCE X/dm-X.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE X/if_X24.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
  list(APPEND libdm_DEFINES DM_OGL IF_OGL)
  set(dmogl_srcs glx/dm-ogl.c glx/if_ogl.c)
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${OPENGL_LIBRARIES})
  set_property(SOURCE glx/dm-ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE glx/if_ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_TK AND NOT WIN32)
  list(APPEND libdm_DEFINES DM_TK IF_TK)
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${TCL_LIBRARY} ${TK_LIBRARY})
  set(dmtk_srcs tk/dm-tk.c tk/if_tk.c)
  set_property(SOURCE tk/dm-tk.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE tk/if_tk.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_TK AND NOT WIN32)

if(BRLCAD_ENABLE_QT)
  CHECK_CXX_FLAG(Wno-float-equal)

  list(APPEND libdm_DEFINES DM_QT IF_QT)
  set(dmqt_srcs qt/dm-qt.cpp qt/if_qt.cpp)
  set_property(SOURCE qt/dm-qt.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE qt/if_qt.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  add_definitions(${Qt5Widgets_DEFINITIONS})
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${Qt5Widgets_LIBRARIES})
endif(BRLCAD_ENABLE_QT)

add_definitions(
  -DIF_REMOTE
  -DIF_AB
  )
if(BRLCAD_ENABLE_OSG AND BRLCAD_ENABLE_TK)
  list(APPEND libdm_DEFINES DM_OSGL IF_OSGL)
  set(dmosg_srcs osgl/dm-osgl.cpp osgl/if_osgl.cpp)
  set_property(SOURCE osgl/dm-osgl.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE osgl/if_osgl.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set(OSG_LIBRARIES
    ${OPENGL_LIBRARIES}
    ${OSG_LIBRARY}
    ${OSGTEXT_LIBRARY}
    ${OSGVIEWER_LIBRARY}
    )
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${OSG_LIBRARIES})
endif(BRLCAD_ENABLE_OSG AND BRLCAD_ENABLE_TK)


set_property(GLOBAL PROPERTY libdm_DEFINES "${libdm_DEFINES}")

set(LIBDM_SRCS
  ${dmx11_srcs}
  ${dmw32_srcs}
  ${dmogl_srcs}
  ${dmosg_srcs}
  ${dmtk_srcs}
  ${dmqt_srcs}
  null/dm-Null.c
  null/if_null.c
  plot/dm-plot.c
  postscript/dm-ps.c
  txt/dm-txt.c
  txt/if_debug.c
  adc.c
  asize.c
  axes.c
  clip.c
  color.c
  dm-generic.c
  dm_obj.c
  dm_util.c
  fb_generic.c
  fb_log.c
  fb_obj.c
  fb_paged_io.c
  fb_rect.c
  fb_util.c
  fbserv_obj.c
  grid.c
  if_disk.c
  if_mem.c
  if_remote.c
  if_stack.c
  knob.c
  labels.c
  options.c
  query.c
  rect.c
  scale.c
  tcl.c
  vers.c
  )
set_property(SOURCE dm_obj.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
BRLCAD_ADDLIB(libdm "${LIBDM_SRCS}" "librt;libbu;libpkg;${DM_EXTRA_LIBS};${PNG_LIBRARIES}")
set_target_properties(libdm PROPERTIES VERSION 20.0.1 SOVERSION 20)

if(BRLCAD_ENABLE_OSG)
  BRLCAD_ADDEXEC(osg-test osg-test.cpp "librt;libbu;${OSG_LIBRARIES}" NO_STRICT NO_INSTALL)
  if(CPP_DLL_DEFINES)
    set_property(TARGET libdm APPEND PROPERTY COMPILE_DEFINITIONS "FREETYPE_DLL_IMPORTS")
  endif(CPP_DLL_DEFINES)
endif(BRLCAD_ENABLE_OSG)

if (TARGET tk)
  add_dependencies(libdm tk)
endif (TARGET tk)
if (TARGET profont_ProFont_ttf_cp)
  add_dependencies(libdm profont_ProFont_ttf_cp)
endif (TARGET profont_ProFont_ttf_cp)

set(libdm_ignore_files
  CMakeLists.txt
  README
  TODO
  X/dm-X.c
  X/dm-X.h
  X/if_X24.c
  dm_private.h
  dm_xvars.h
  fontstash/LICENSE.fontstash
  fontstash/README.md
  fontstash/fontstash.h
  fontstash/glfontstash.h
  fontstash/stb_truetype.h
  glx/dm-glx.h
  glx/dm-ogl.c
  glx/dm-ogl.h
  glx/if_ogl.c
  if_TEMPLATE.c
  null/dm-Null.h
  osgl/dm-osgl.cpp
  osgl/dm-osgl.h
  osgl/if_osgl.cpp
  osgl/osg-test.cpp
  osgl/osg_bob.cpp
  osgl/osg_fb_manipulator.h
  plot/dm-plot.h
  postscript/dm-ps.h
  qt/dm-qt.cpp
  qt/dm-qt.h
  qt/if_qt.cpp
  tk/dm-tk.c
  tk/dm-tk.h
  tk/if_tk.c
  wgl/dm-wgl.c
  wgl/dm-wgl.h
  wgl/if_wgl.c
  include/calltable.h
  )
CMAKEFILES(${libdm_ignore_files})
CMAKEFILES(dm_private.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
