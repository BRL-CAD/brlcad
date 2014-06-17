set(FB_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  ${OPENNURBS_INCLUDE_DIR}
  ${OPENTHREADS_INCLUDE_DIR}
  ${OSG_INCLUDE_DIR}
  ${Qt5Widgets_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(fb FB_INCLUDE_DIRS "")

set(LINKLIBS "")

if(BRLCAD_ENABLE_OSG)
  set(FBOSGDEFS "-DIF_OSG")
  set(LINKLIBS ${LINKLIBS} ${OSG_LIBRARIES})
  set(fbosg_srcs if_osg.cpp)
endif()

if(BRLCAD_ENABLE_QT)
  CHECK_CXX_FLAG(Wno-float-equal)

  set(FBQTDEFS "-DIF_QT")
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  add_definitions(${Qt5Widgets_DEFINITIONS})
  set(LINKLIBS ${LINKLIBS} ${Qt5Widgets_LIBRARIES})
  set(fbqt_srcs if_qt.cpp)
endif(BRLCAD_ENABLE_QT)

set(LIBFB_SOURCES
  ${fbosg_srcs}
  ${fbqt_srcs}
  asize.c
  fb_generic.c
  fb_log.c
  fb_obj.c
  fb_paged_io.c
  fb_rect.c
  fb_util.c
  fbserv_obj.c
  if_debug.c
  if_disk.c
  if_mem.c
  if_null.c
  if_remote.c
  if_stack.c
  tcl.c
  vers.c
  )

add_definitions(
  -DIF_REMOTE
  -DIF_AB
  ${FBX11DEFS}
  ${FBOGLDEFS}
  ${FBOSGDEFS}
  ${FBTKDEFS}
  ${XOPEN_DEFINE}
  ${FBQTDEFS}
  )

BRLCAD_ADDLIB(libfb "${LIBFB_SOURCES}" "libbu;libpkg;${LINKLIBS}")
SET_TARGET_PROPERTIES(libfb PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)
  ADD_DEPENDENCIES(libfb tk)
endif(BRLCAD_BUILD_LOCAL_TK AND BRLCAD_ENABLE_TK)

set(libfb_ignore_files
  dmdfb.h
  if_TEMPLATE.c
  if_osg.cpp
  if_qt.cpp
  )
CMAKEFILES(${libfb_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
