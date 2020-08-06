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

# Backend implementations
add_subdirectory(X)
add_subdirectory(glx)
add_subdirectory(osgl)
add_subdirectory(plot)
add_subdirectory(postscript)
add_subdirectory(qt)
add_subdirectory(tk)
add_subdirectory(txt)
add_subdirectory(wgl)

# Main libdm library
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

if (TARGET profont_ProFont_ttf_cp)
  add_dependencies(libdm profont_ProFont_ttf_cp)
endif (TARGET profont_ProFont_ttf_cp)

add_subdirectory(tests)

set(libdm_ignore_files
  CMakeLists.txt
  README
  TODO
  if_TEMPLATE.c
  null/dm-Null.h
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
