if(BRLCAD_ENABLE_TCL)
  if(BRLCAD_ENABLE_TK)
    set(TCL_ENABLE_TK ON)
  endif(BRLCAD_ENABLE_TK)
  find_package_tcl(REQUIRED)

  function(tclcad_package_version result package_name)
    file(
      GLOB package_dirs
      LIST_DIRECTORIES true
      RELATIVE "${CMAKE_BINARY_DIR}/${LIB_DIR}"
      "${CMAKE_BINARY_DIR}/${LIB_DIR}/${package_name}4.*"
      )
    list(FILTER package_dirs INCLUDE REGEX "^${package_name}4\\.[0-9]+(\\.[0-9]+)*$")
    if(package_dirs)
      list(SORT package_dirs COMPARE NATURAL ORDER DESCENDING)
      list(GET package_dirs 0 package_dir)
      string(REGEX REPLACE "^${package_name}" "" package_version "${package_dir}")
      set(${result} "${package_version}" PARENT_SCOPE)
    endif(package_dirs)
  endfunction(tclcad_package_version)

  tclcad_package_version(ITCL_VERSION itcl)
  tclcad_package_version(ITK_VERSION itk)
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
    set(TCLCAD_INCLUDE_DIRS ${TCLCAD_INCLUDE_DIRS} ${TK_INCLUDE_PATH} ${BRLCAD_X11_INCLUDE_DIRS})
  endif(TK_INCLUDE_PATH)

  # Include directories only needed by the implementation
  set(TCLCAD_LOCAL_INCLUDE_DIRS ${PNG_INCLUDE_DIRS})
  if(BRLCAD_ENABLE_X11)
    set(TCLCAD_XLIBS ${TCLCAD_XLIBS} ${BRLCAD_X11_LIBRARIES})
  endif(BRLCAD_ENABLE_X11)

  # Note - libtclcad_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
  set(TCLCAD_PUBLIC_LIBS ${libtclcad_deps} ${TCL_LIBRARY})
  set(TCLCAD_PRIVATE_LIBS ${TCLCAD_XLIBS} PNG::PNG)
  if(TK_LIBRARY)
    set(TCLCAD_PUBLIC_LIBS ${TCLCAD_PUBLIC_LIBS} ${TK_LIBRARY})
  endif(TK_LIBRARY)

  brlcad_addlib(libtclcad "${LIBTCLCAD_SRCS}" "${TCLCAD_INCLUDE_DIRS}" "${TCLCAD_LOCAL_INCLUDE_DIRS}"
    PUBLIC_LIBS ${TCLCAD_PUBLIC_LIBS}
    PRIVATE_LIBS ${TCLCAD_PRIVATE_LIBS}
    # refresh.c defines DM_WITH_RT before including dm/view.h to expose
    # dm_draw_viewobjs; when batched after draw.c the DM_VIEW_H guard fires
    # without DM_WITH_RT set and the declaration is lost.
    # bu.c and dm.c both define a non-static register_cmds function.
    UNITY_BUILD_SKIP view/refresh.c bu.c dm.c
    # The full static library link is too large for some testing environments -
    # disable this test
    NO_STATIC_LINK_TEST
  )
  set(ptargets libtclcad)
  if(TARGET libtclcad-obj)
    list(APPEND ptargets libtclcad-obj)
  endif(TARGET libtclcad-obj)
  foreach(pt ${ptargets})
    if(ITCL_VERSION)
      target_compile_definitions(${pt} PRIVATE ITCL_VERSION=\"${ITCL_VERSION}\")
    endif(ITCL_VERSION)
    if(ITK_VERSION)
      target_compile_definitions(${pt} PRIVATE ITK_VERSION=\"${ITK_VERSION}\")
    endif(ITK_VERSION)
    if(IWIDGETS_VERSION)
      target_compile_definitions(${pt} PRIVATE IWIDGETS_VERSION=\"${IWIDGETS_VERSION}\")
    endif(IWIDGETS_VERSION)
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
