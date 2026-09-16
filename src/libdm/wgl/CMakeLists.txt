set(WGL_SRCS dm-wgl.c if_wgl.c)

if(WIN32 AND BRLCAD_ENABLE_OPENGL AND OPENGL_TARGETS AND BRLCAD_ENABLE_TK)

  set(TCL_ENABLE_TK ON)
  find_package_tcl(TCL)

  if(TCL_FOUND AND TK_FOUND)
    include_directories(
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${BRLCAD_BINARY_DIR}/include
      ${BRLCAD_SOURCE_DIR}/include
      ${TCL_INCLUDE_PATH}
      ${TK_INCLUDE_PATH}
      # Private Tk headers needed for the internal Tk function TkWinGetHWND
      ${CMAKE_CURRENT_SOURCE_DIR}/wintk
    )

    set_property(SOURCE dm-wgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
    set_property(SOURCE if_wgl.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

    add_definitions(-DDM_PLUGIN)

    dm_plugin_library(dm-wgl SHARED ${WGL_SRCS})
    target_link_libraries(
      dm-wgl
      libdmgl
      libdm
      libbu
      ${TCL_LIBRARY}
      ${TK_LIBRARY}
      ${OPENGL_TARGETS}
    )
    set_property(TARGET dm-wgl APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
    validate_style(dm-wgl "${WGL_SRCS}")

    plugin_setup(dm-wgl dm)
  endif(TCL_FOUND AND TK_FOUND)
endif()

cmakefiles(
  CMakeLists.txt
  ${WGL_SRCS}
  dm-wgl.h
  fb_wgl.h
  wintk/tkWinPort.h
  wintk/tkIntDecls.h
  wintk/tkWinInt.h
  wintk/tkInt.h
  wintk/tkWin.h
  wintk/tkIntPlatDecls.h
  wintk/tkPort.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
