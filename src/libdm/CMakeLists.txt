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
  ${Qt5Widgets_INCLUDE_DIRS}
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
endif(WIN32 AND BRLCAD_ENABLE_OPENGL)

if(BRLCAD_ENABLE_X11)
  set(DM_X_LIBS ${X11_LIBRARIES} ${X11_Xi_LIB})
endif(BRLCAD_ENABLE_X11)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)
  list(APPEND libdm_DEFINES DM_X IF_X)
  set(dmx11_srcs dm-X.c)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
  list(APPEND libdm_DEFINES DM_OGL IF_OGL)
  set(dmogl_srcs dm-ogl.c)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_RTGL)
  list(APPEND libdm_DEFINES DM_RTGL IF_RTGL)
  set(dmrtgl_srcs dm-rtgl.c)
endif(BRLCAD_ENABLE_RTGL)

if(BRLCAD_ENABLE_TK AND NOT WIN32)
  list(APPEND libdm_DEFINES DM_TK IF_TK)
  set(DM_TKLIB ${TCL_TK_LIBRARY})
  set(dmtk_srcs dm-tk.c)
endif(BRLCAD_ENABLE_TK AND NOT WIN32)

if(BRLCAD_ENABLE_QT)
  CHECK_CXX_FLAG(Wno-float-equal)

  list(APPEND libdm_DEFINES DM_QT IF_QT)
  set(dmqt_srcs dm-qt.cpp)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  add_definitions(${Qt5Widgets_DEFINITIONS})
endif(BRLCAD_ENABLE_QT)

if(BRLCAD_ENABLE_OSG)
  list(APPEND libdm_DEFINES DM_OSG)
  set(dmosg_srcs dm-osg.cpp)
  set(OSG_LIBRARIES
    ${OSG_LIBRARY}
    ${OSGTEXT_LIBRARY}
    ${OSGVIEWER_LIBRARY}
    )
endif(BRLCAD_ENABLE_OSG)


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

BRLCAD_ADDLIB(libdm "${LIBDM_SRCS}" "libged;librt;libfb;${DM_X_LIBS};${DM_TKLIB};${Qt5Widgets_LIBRARIES};${OSG_LIBRARIES}")
set_target_properties(libdm PROPERTIES VERSION 20.0.1 SOVERSION 20)

if(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
  add_dependencies(libdm tk)
endif(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
set(libdm_ignore_files
  dm-wgl.c
  dm-X.c
  dm-ogl.c
  dm-osg.cpp
  dm-qt.cpp
  dm-rtgl.c
  dm-tk.c
  osg.cpp
  TODO
  )
CMAKEFILES(${libdm_ignore_files})
CMAKEFILES(dm_util.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
