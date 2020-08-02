# Include directories needed by libdm users
set(DM_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${ICV_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${X11_INCLUDE_DIR}
  ${PNG_INCLUDE_DIRS}
  )

# local includes
set(DM_LOCAL_INCLUDE_DIRS 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_SOURCE_DIR}/src/libbg
  )

BRLCAD_LIB_INCLUDE_DIRS(dm DM_INCLUDE_DIRS DM_LOCAL_INCLUDE_DIRS)

if(BRLCAD_ENABLE_TCL)
  add_subdirectory(plot)
  add_subdirectory(txt)
endif(BRLCAD_ENABLE_TCL)

# We no longer use these in libdm, but MGED hasn't caught up yet...
set(libdm_DEFINES "")

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
  add_subdirectory(glx)
  list(APPEND libdm_DEFINES DM_OGL IF_OGL)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)
  add_subdirectory(X)
  list(APPEND libdm_DEFINES DM_X IF_X)
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)
if(WIN32 AND BRLCAD_ENABLE_OPENGL)
  add_subdirectory(wgl)
  list(APPEND libdm_DEFINES DM_WGL IF_WGL)
endif(WIN32 AND BRLCAD_ENABLE_OPENGL)
if(BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_DM_TK)
  add_subdirectory(tk)
  list(APPEND libdm_DEFINES DM_TK IF_TK)
endif(BRLCAD_ENABLE_TK AND BRLCAD_ENABLE_DM_TK)
if(BRLCAD_ENABLE_OSG AND BRLCAD_ENABLE_TK)
  add_subdirectory(osgl)
  list(APPEND libdm_DEFINES DM_OSGL IF_OSGL)
endif(BRLCAD_ENABLE_OSG AND BRLCAD_ENABLE_TK)
if(BRLCAD_ENABLE_QT)
  add_subdirectory(qt)
  list(APPEND libdm_DEFINES DM_QT IF_QT)
endif(BRLCAD_ENABLE_QT)

# put the defines where MGED's build can find them...
set_property(GLOBAL PROPERTY libdm_DEFINES "${libdm_DEFINES}")

set(LIBDM_SRCS
  null/dm-Null.c
  null/if_null.c
  txt/if_debug.c
  adc.c
  asize.c
  axes.c
  bview_util.c
  clip.c
  dm-generic.c
  dm_init.cpp
  dm_plugins.cpp
  dm_util.c
  fb_generic.c
  fb_log.c
  fb_paged_io.c
  fb_rect.c
  fb_util.c
  grid.c
  if_disk.c
  if_mem.c
  if_remote.c
  if_stack.c
  knob.c
  labels.c
  options.c
  rect.c
  scale.c
  vers.c
  )
set_property(SOURCE dm_obj.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
set_property(SOURCE dm_init.cpp APPEND PROPERTY COMPILE_DEFINITIONS "DM_PLUGIN_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")
BRLCAD_ADDLIB(libdm "${LIBDM_SRCS}" "librt;libicv;libbu;libpkg;${OPENGL_LIBRARIES};${PNG_LIBRARIES}")
set_target_properties(libdm PROPERTIES VERSION 20.0.1 SOVERSION 20)

#if(BRLCAD_ENABLE_OSG)
#  BRLCAD_ADDEXEC(osg-test osgl/osg-test.cpp "librt;libbu;${OSG_LIBRARIES}" NO_STRICT NO_INSTALL)
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

if (TARGET profont_ProFont_ttf_cp)
  add_dependencies(libdm profont_ProFont_ttf_cp)
endif (TARGET profont_ProFont_ttf_cp)

add_subdirectory(tests)

set(libdm_ignore_files
  CMakeLists.txt
  README
  TODO
  X/CMakeLists.txt
  X/color.c
  X/dm-X.c
  X/dm-X.h
  X/fb_X.h
  X/if_X24.c
  glx/CMakeLists.txt
  glx/dm-glx.h
  glx/dm-ogl.c
  glx/dm-ogl.h
  glx/fb_ogl.h
  glx/if_ogl.c
  if_TEMPLATE.c
  null/dm-Null.h
  osgl/CMakeLists.txt
  osgl/dm-osgl.cpp
  osgl/dm-osgl.h
  osgl/fb_osgl.h
  osgl/fontstash/LICENSE.fontstash
  osgl/fontstash/README.md
  osgl/fontstash/fontstash.h
  osgl/fontstash/glfontstash.h
  osgl/fontstash/stb_truetype.h
  osgl/if_osgl.cpp
  osgl/osg-test.cpp
  osgl/osg_bob.cpp
  osgl/osg_fb_manipulator.h
  plot/CMakeLists.txt
  plot/dm-plot.c
  plot/dm-plot.h
  postscript/CMakeLists.txt
  postscript/dm-ps.c
  postscript/dm-ps.h
  qt/CMakeLists.txt
  qt/dm-qt.cpp
  qt/dm-qt.h
  qt/fb_qt.h
  qt/if_qt.cpp
  tk/CMakeLists.txt
  tk/dm-tk.c
  tk/dm-tk.h
  tk/if_tk.c
  txt/CMakeLists.txt
  txt/dm-txt.c
  wgl/CMakeLists.txt
  wgl/dm-wgl.c
  wgl/dm-wgl.h
  wgl/fb_wgl.h
  wgl/if_wgl.c
  include/private.h
  include/calltable.h
  )
CMAKEFILES(${libdm_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
