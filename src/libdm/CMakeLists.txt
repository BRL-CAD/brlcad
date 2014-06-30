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
  ${OPENTHREADS_INCLUDE_DIR}
  ${OSG_INCLUDE_DIR}
  )

#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-as-needed")

# local includes
set(DM_LOCAL_INCLUDE_DIRS ${PNG_INCLUDE_DIR})

BRLCAD_LIB_INCLUDE_DIRS(dm DM_INCLUDE_DIRS DM_LOCAL_INCLUDE_DIRS)

# Initialize libdm_DEFINES in case of reconfiguration
set(libdm_DEFINES "")

# Set libdm compilation definitions
get_property(libdm_DEFINES GLOBAL PROPERTY libdm_DEFINES)

if(BRLCAD_ENABLE_QT)
  CHECK_CXX_FLAG(Wno-float-equal)

  list(APPEND libdm_DEFINES DM_QT IF_QT)
  set(dmqt_srcs dm-qt.cpp)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  add_definitions(${Qt5Widgets_DEFINITIONS})
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${Qt5Widgets_LIBRARIES})
endif(BRLCAD_ENABLE_QT)

if(BRLCAD_ENABLE_OSG)
  list(APPEND libdm_DEFINES DM_OSG IF_OSG)
  set(dmosg_srcs dm-osg.cpp)
  set(DM_EXTRA_LIBS ${DM_EXTRA_LIBS} ${OSG_LIBRARIES} ${OPENGL_LIBRARY}  ${TCL_TK_LIBRARY})
endif(BRLCAD_ENABLE_OSG)


set_property(GLOBAL PROPERTY libdm_DEFINES "${libdm_DEFINES}")

set(LIBDM_SRCS
  ${dmosg_srcs}
  ${dmqt_srcs}
  adc.c
  axes.c
  clip.c
  color.c
  dm-Null.c
  dm-generic.c
  dm-plot.c
  dm-ps.c
  dm_obj.cpp
  dm_util.c
  dm-txt.c
  globals.c
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

BRLCAD_ADDLIB(libdm "${LIBDM_SRCS}" "librt;libfb;${DM_X_LIBS};${DM_TKLIB};${PNG_LIBRARY};${DM_EXTRA_LIBS}")
set_target_properties(libdm PROPERTIES VERSION 20.0.1 SOVERSION 20)

if(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
  add_dependencies(libdm tk)
endif(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_OSG)
  BRLCAD_ADDEXEC(osg-test osg-test.cpp "librt;libbu;${OSG_LIBRARIES}" NO_STRICT NO_INSTALL)
endif(BRLCAD_ENABLE_OSG)

set(libdm_ignore_files
  dm-osg.cpp
  dm-qt.cpp
  osg.cpp
  osg-test.cpp
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
