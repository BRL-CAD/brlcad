# Include directories needed by libdm users
set(DM_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${OPENTHREADS_INCLUDE_DIR}
  ${OSG_INCLUDE_DIR}
  )

# local includes
set(DM_LOCAL_INCLUDE_DIRS ${PNG_INCLUDE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(dm DM_INCLUDE_DIRS DM_LOCAL_INCLUDE_DIRS)

# Initialize libdm_DEFINES in case of reconfiguration
set(libdm_DEFINES "")

# Set libdm compilation definitions
get_property(libdm_DEFINES GLOBAL PROPERTY libdm_DEFINES)
# Add the individual OS specific files and definitions
if(WIN32 AND BRLCAD_ENABLE_OPENGL)
  list(APPEND libdm_DEFINES DM_WGL IF_WGL)
  set(dmw32_srcs dm-wgl.c)
  set_property(SOURCE dm-wgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(WIN32 AND BRLCAD_ENABLE_OPENGL)

if(BRLCAD_ENABLE_X11)
  set(DM_X_LIBS ${X11_LIBRARIES} ${X11_Xi_LIB})
endif(BRLCAD_ENABLE_X11)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)
  list(APPEND libdm_DEFINES DM_X IF_X)
  set(dmx11_srcs dm-X.c)
  set_property(SOURCE dm-X.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
  list(APPEND libdm_DEFINES DM_OGL IF_OGL)
  set(dmogl_srcs dm-ogl.c)
  set_property(SOURCE dm-ogl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_RTGL)
  list(APPEND libdm_DEFINES DM_RTGL IF_RTGL)
  set(dmrtgl_srcs dm-rtgl.c)
  set_property(SOURCE dm-rtgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_RTGL)

if(BRLCAD_ENABLE_TK AND NOT WIN32)
  list(APPEND libdm_DEFINES DM_TK IF_TK)
  set(DM_TKLIB ${TCL_TK_LIBRARY})
  set(dmtk_srcs dm-tk.c)
  set_property(SOURCE dm-tk.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
endif(BRLCAD_ENABLE_TK AND NOT WIN32)

if(BRLCAD_ENABLE_QT)
  CHECK_CXX_FLAG(Wno-float-equal)

  list(APPEND libdm_DEFINES DM_QT IF_QT)
  set(dmqt_srcs dm-qt.cpp)
  set_property(SOURCE dm-qt.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  add_definitions(${Qt5Widgets_DEFINITIONS})
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${Qt5Widgets_LIBRARIES})
endif(BRLCAD_ENABLE_QT)

if(BRLCAD_ENABLE_OSG AND BRLCAD_ENABLE_TK)
  list(APPEND libdm_DEFINES DM_OSGL IF_OSGL)
  set(dmosg_srcs dm-osgl.cpp)
  set_property(SOURCE dm-osgl.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set(OSG_LIBRARIES
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
  ${dmrtgl_srcs}
  ${dmtk_srcs}
  ${dmqt_srcs}
  adc.c
  axes.c
  clip.c
  color.c
  dm-Null.c
  dm-generic.c
  dm-plot.c
  dm-ps.c
  dm_obj.c
  dm_util.c
  dm-txt.c
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
set_property(SOURCE dm_obj.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
BRLCAD_ADDLIB(libdm "${LIBDM_SRCS}" "librt;libfb;${DM_X_LIBS};${DM_TKLIB};${DM_EXTRA_LIBS};${PNG_LIBRARY}")
set_target_properties(libdm PROPERTIES VERSION 20.0.1 SOVERSION 20)

if(BRLCAD_ENABLE_OSG)
  BRLCAD_ADDEXEC(osg-test osg-test.cpp "librt;libbu;${OSG_LIBRARIES}" NO_STRICT NO_INSTALL)
  if(CPP_DLL_DEFINES)
    set_property(TARGET libdm APPEND PROPERTY COMPILE_DEFINITIONS "FREETYPE_DLL_IMPORTS")
  endif(CPP_DLL_DEFINES)
endif(BRLCAD_ENABLE_OSG)

if(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
  add_dependencies(libdm tk)
endif(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
if(BRLCAD_ENABLE_OSG)
  add_dependencies(libdm profont_ProFont_ttf_cp)
endif(BRLCAD_ENABLE_OSG)
set(libdm_ignore_files
  README
  TODO
  dm-Null.h
  dm-X.c
  dm-X.h
  dm-glx.h
  dm-ogl.c
  dm-ogl.h
  dm-osgl.cpp
  dm-osgl.h
  dm-pex.h
  dm-plot.h
  dm-ps.h
  dm-qt.cpp
  dm-qt.h
  dm-rtgl.c
  dm-rtgl.h
  dm-tk.c
  dm-tk.h
  dm-wgl.c
  dm-wgl.h
  dm_private.h
  fontstash
  osg-test.cpp
  osg_bob.cpp
  )
CMAKEFILES(${libdm_ignore_files})
CMAKEFILES(dm_private.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
