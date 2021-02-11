set(OSGL_SRCS
  dm-osgl.cpp
  if_osgl.cpp
  )

if(BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_OSG)

  # Make sure we know about required components
  find_package(X11)
  find_package(GL)
  #find_package(OpenThreads)
  #find_package(OpenSceneGraph)
  find_package(TCL)

  # Include directories needed by libdm users
  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR_GL}
    ${OPENGL_INCLUDE_DIR_GLX}
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
    ${OPENTHREADS_INCLUDE_DIR}
    ${OSG_INCLUDE_DIR}
    )

  set(OSGL_LIBRARIES
    ${OPENGL_LIBRARIES}
    ${TCL_LIBRARY}
    ${TK_LIBRARY}
    ${OSG_LIBRARY}
    ${OSGTEXT_LIBRARY}
    ${OSGVIEWER_LIBRARY}
    )

  set_property(SOURCE dm-osgl.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
  set_property(SOURCE if_osgl.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

  add_definitions(-DDM_PLUGIN)

  dm_plugin_library(dm-osgl SHARED ${OSGL_SRCS})
  target_link_libraries(dm-osgl libdm librt libbu ${OSGL_LIBRARIES})
  set_property(TARGET dm-osgl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(dm-osgl "${OSGL_SRCS}")

  PLUGIN_SETUP(dm-osgl dm)

endif(BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_OSG)

#if(BRLCAD_ENABLE_OSG)
#  BRLCAD_ADDEXEC(osg-test osg-test.cpp "librt;libbu;${OSG_LIBRARIES}" NO_STRICT NO_INSTALL)
#  if(HIDE_INTERNAL_SYMBOLS)
#    if (TARGET osg OR HIDE_INTERNAL_SYMBOLS_EXT)
#      set_property(TARGET libdm APPEND PROPERTY COMPILE_DEFINITIONS "FREETYPE_DLL_IMPORTS")
#    endif (TARGET osg OR HIDE_INTERNAL_SYMBOLS_EXT)
#    if(TARGET libdm-obj)
#      if (TARGET osg OR HIDE_INTERNAL_SYMBOLS_EXT)
#        set_property(TARGET libdm-obj APPEND PROPERTY COMPILE_DEFINITIONS "FREETYPE_DLL_IMPORTS")
#      endif (TARGET osg OR HIDE_INTERNAL_SYMBOLS_EXT)
#    endif(TARGET libdm-obj)
#  endif(HIDE_INTERNAL_SYMBOLS)
#endif(BRLCAD_ENABLE_OSG)

CMAKEFILES(
  ${OSGL_SRCS}
  CMakeLists.txt
  dm-osgl.h
  fb_osgl.h
  osg-test.cpp
  osg_bob.cpp
  osg_fb_manipulator.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
