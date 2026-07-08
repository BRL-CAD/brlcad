# Main libdm library
set(
  LIBDM_SRCS
  null/dm-Null.c
  null/if_null.c
  txt/if_debug.c
  adc.c
  asize.c
  axes.c
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
  fbserv.c
  grid.c
  if_disk.c
  if_mem.c
  if_remote.c
  if_stack.c
  labels.c
  options.c
  rect.c
  scale.c
  view.c
  vers.c
)

set_property(SOURCE dm_obj.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
set_property(
  SOURCE dm_init.cpp
  APPEND
  PROPERTY COMPILE_DEFINITIONS "DM_PLUGIN_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\""
)

brlcad_find_package(PNG REQUIRED)
find_package(OpenSSL QUIET)

set(DM_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PNG_INCLUDE_DIRS}
  ${BRLCAD_SOURCE_DIR}/src/libbg
)

set(LIBDM_PUBLIC_LIBS ${libdm_deps})
if(OpenSSL_FOUND OR OPENSSL_FOUND)
  list(APPEND LIBDM_PUBLIC_LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

# Note - libdm_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
brlcad_addlib(libdm "${LIBDM_SRCS}" "" "${DM_LOCAL_INCLUDE_DIRS}"
  PUBLIC_LIBS ${LIBDM_PUBLIC_LIBS}
  PRIVATE_LIBS PNG::PNG ${WINSOCK_LIB}
  UNITY_BUILD_SKIP dm_init.cpp
)
set_target_properties(libdm PROPERTIES
  VERSION 20.0.1
  SOVERSION 20
)

if(OpenSSL_FOUND OR OPENSSL_FOUND)
  target_compile_definitions(libdm PRIVATE HAVE_OPENSSL_SSL_H=1)
  if(TARGET libdm-obj)
    target_compile_definitions(libdm-obj PRIVATE HAVE_OPENSSL_SSL_H=1)
  endif()
  if(TARGET libdm-brlcad-obj)
    target_compile_definitions(libdm-brlcad-obj PRIVATE HAVE_OPENSSL_SSL_H=1)
    brlcad_register_aggregate_link_libs(OpenSSL::SSL OpenSSL::Crypto)
  endif()
endif()

if(TARGET profont_ProFont_ttf_cp)
  add_dependencies(libdm profont_ProFont_ttf_cp)
endif()

# Many of the backends use the same OpenGL drawing logic for much of their
# functionality. If OpenGL support is available, encapsulate the common
# elements of this logic in a libdmgl library used by those plugins.
set(LIBDMGL_SRCS
  dm-gl.c
  dm-gl_lod.cpp
)

if(BRLCAD_ENABLE_OPENGL AND OPENGL_TARGETS)
  set(_libdmgl_private_libs
    libdm
    libbn
    libbu
    ${OPENGL_TARGETS}
  )

  message("OPENGL_TARGETS: ${OPENGL_TARGETS}")

  brlcad_addlib(libdmgl "${LIBDMGL_SRCS}" "" "${DM_LOCAL_INCLUDE_DIRS}"
    PRIVATE_LIBS ${_libdmgl_private_libs}
  )
  set_target_properties(libdmgl PROPERTIES
    VERSION 20.0.1
    SOVERSION 20
  )
endif()

add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/dm_plugins.sentinel"
  COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/dm_plugins.sentinel"
)
add_custom_target(dm_plugins_sentinel DEPENDS "${CMAKE_BINARY_DIR}/dm_plugins.sentinel")
set_target_properties(dm_plugins_sentinel PROPERTIES FOLDER "BRL-CAD Plugins/dm")

function(dm_plugin_library name)
  add_library(${name} ${ARGN})
  add_dependencies(dm_plugins_sentinel ${name})
  # On some platforms distclean has additional cleanup to do
  distclean("${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/dm/${name}.exp")
  distclean("${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/dm/${name}.pdb")
endfunction()

# This target is supplied so applications wanting to run libdm
# commands at build time can depend on the plugins being built as well as
# libdm itself (otherwise LIBGED commands might fail due to their plugins not
# yet having been built.)
add_custom_target(dm_plugins ALL DEPENDS dm_plugins_sentinel)
set_target_properties(dm_plugins PROPERTIES FOLDER "BRL-CAD Plugins")

# Backend implementations
add_subdirectory(X)
add_subdirectory(glx)
add_subdirectory(plot)
add_subdirectory(postscript)
add_subdirectory(qtgl)
add_subdirectory(swrast)
add_subdirectory(tkswrast)
add_subdirectory(txt)
add_subdirectory(wgl)

add_subdirectory(tests)

set(
  libdm_ignore_files
  CMakeLists.txt
  README
  TODO
  dm-gl.c
  dm-gl.h
  if_TEMPLATE.c
  null/dm-Null.h
  include/private.h
  include/calltable.h
  fontstash/LICENSE.fontstash
  fontstash/README.md
  fontstash/fontstash.h
  fontstash/glfontstash.h
  fontstash/stb_truetype.h
)
cmakefiles(${libdm_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
