if(BRLCAD_ENABLE_TCL)
  if(BRLCAD_ENABLE_TK)
    set(TCL_ENABLE_TK ON)
  endif(BRLCAD_ENABLE_TK)
  find_package_tcl(REQUIRED)
  if(EXISTS ${CMAKE_BINARY_DIR}/${LIB_DIR}/itcl3.4)
    set(ITCL_VERSION "3.4")
  endif(EXISTS ${CMAKE_BINARY_DIR}/${LIB_DIR}/itcl3.4)
  if(EXISTS ${CMAKE_BINARY_DIR}/${LIB_DIR}/itk3.4)
    set(ITK_VERSION "3.4")
  endif(EXISTS ${CMAKE_BINARY_DIR}/${LIB_DIR}/itk3.4)
  if(EXISTS ${CMAKE_BINARY_DIR}/${LIB_DIR}/Iwidgets4.1.1)
    set(IWIDGETS_VERSION "4.1.1")
  endif(EXISTS ${CMAKE_BINARY_DIR}/${LIB_DIR}/Iwidgets4.1.1)
endif()

set(
  LIBTCLCAD_SRCS
  auto_path.c
  bn.c
  bu.c
  command_io.cpp
  commands.c
  cmdhist.c
  dm.c
  eval.c
  fb.c
  fbserv.c
  global.c
  init.c
  mouse.c
  polygons.c
  rt.c
  tkImgFmtPIX.c
  view/arrows.c
  view/autoview.c
  view/axes.c
  view/draw.c
  view/faceplate.c
  view/labels.c
  view/lines.c
  view/refresh.c
  view/util.c
  wrapper.c
)

if(BRLCAD_ENABLE_TCL)
  # Include directories needed by libtclcad users
  set(TCLCAD_INCLUDE_DIRS ${TCL_INCLUDE_PATH})
  if(TK_INCLUDE_PATH)
    set(TCLCAD_INCLUDE_DIRS ${TCLCAD_INCLUDE_DIRS} ${TK_INCLUDE_PATH} ${X11_INCLUDE_DIR})
  endif(TK_INCLUDE_PATH)

  # Include directories only needed by the implementation
  set(TCLCAD_LOCAL_INCLUDE_DIRS ${PNG_INCLUDE_DIRS})
  if(BRLCAD_ENABLE_X11)
    set(TCLCAD_XLIBS ${TCLCAD_XLIBS} ${X11_LIBRARIES})
  endif(BRLCAD_ENABLE_X11)

  # Note - libtclcad_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
  set(TCLCAD_LIBS ${libtclcad_deps} ${TCL_LIBRARY} ${TCLCAD_XLIBS})
  if(TK_LIBRARY)
    set(TCLCAD_LIBS ${TCLCAD_LIBS} ${TK_LIBRARY})
  endif(TK_LIBRARY)

  brlcad_addlib(libtclcad "${LIBTCLCAD_SRCS}" "${TCLCAD_LIBS}" "${TCLCAD_INCLUDE_DIRS}" "${TCLCAD_LOCAL_INCLUDE_DIRS}")
  set(ptargets libtclcad)
  if(TARGET libtclcad-obj)
    list(APPEND ptargets libtclcad-obj)
  endif(TARGET libtclcad-obj)
  foreach(pt ${ptargets})
    target_compile_definitions(${pt} PRIVATE ITCL_VERSION=\"${ITCL_VERSION}\")
    target_compile_definitions(${pt} PRIVATE ITK_VERSION=\"${ITK_VERSION}\")
    target_compile_definitions(${pt} PRIVATE IWIDGETS_VERSION=\"${IWIDGETS_VERSION}\")
    if(TK_LIBRARY)
      target_compile_definitions(${pt} PRIVATE HAVE_TK)
    endif(TK_LIBRARY)
  endforeach(pt ${ptargets})

  set_target_properties(libtclcad PROPERTIES VERSION 20.0.1 SOVERSION 20)
  if(TARGET itcl_pkgIndex)
    add_dependencies(libtclcad itcl_pkgIndex)
  endif(TARGET itcl_pkgIndex)
  if(TARGET itk_pkgIndex)
    add_dependencies(libtclcad itk_pkgIndex)
  endif(TARGET itk_pkgIndex)

  # For anyone to do anything with tclcad, we need the libged plugins
  add_dependencies(libtclcad ged_plugins)
endif(BRLCAD_ENABLE_TCL)

add_subdirectory(tests)

cmakefiles(
  ${LIBTCLCAD_SRCS}
  tclcad_private.h
  view/view.h
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
