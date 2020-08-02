set(LIBTCLCAD_SRCS
  auto_path.c
  bn.c
  bu.c
  cmdhist.c
  dm.c
  fb.c
  fbserv.c
  rt.c
  tclcad_eval.c
  tclcad_global.c
  tclcad_init.c
  tclcad_mouse.c
  tclcad_obj.c
  tclcad_obj_wrapper.c
  tclcad_polygons.c
  view/arrows.c
  view/autoview.c
  view/axes.c
  view/draw.c
  view/faceplate.c
  view/labels.c
  view/lines.c
  view/refresh.c
  view/util.c
  tkImgFmtPIX.c
  )

if (BRLCAD_ENABLE_TCL)

  # Include directories needed by libtclcad users
  set(TCLCAD_INCLUDE_DIRS
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${DM_INCLUDE_DIRS}
    ${NMG_INCLUDE_DIRS}
    ${FB_INCLUDE_DIRS}
    ${GED_INCLUDE_DIRS}
    ${BN_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
    )

  # Include directories only needed by the implementation
  set(TCLCAD_LOCAL_INCLUDE_DIRS
    ${PNG_INCLUDE_DIRS}
    )

  BRLCAD_LIB_INCLUDE_DIRS(tclcad TCLCAD_INCLUDE_DIRS TCLCAD_LOCAL_INCLUDE_DIRS)

  if(BRLCAD_ENABLE_X11)
    set(TCLCAD_XLIBS ${TCLCAD_XLIBS} ${X11_LIBRARIES})
  endif(BRLCAD_ENABLE_X11)

  BRLCAD_ADDLIB(libtclcad "${LIBTCLCAD_SRCS}" "libged;libdm;libdm;libnmg;libbn;libbu;${TCL_LIBRARY};${TK_LIBRARY};${ITCL_LIBRARY};${ITK_LIBRARY};${TCLCAD_XLIBS}")
  set_target_properties(libtclcad PROPERTIES VERSION 20.0.1 SOVERSION 20)
  if (TARGET itcl_pkgIndex)
    add_dependencies(libtclcad itcl_pkgIndex)
  endif (TARGET itcl_pkgIndex)
  if (TARGET itk_pkgIndex)
    add_dependencies(libtclcad itk_pkgIndex)
  endif (TARGET itk_pkgIndex)

  # For anyone to do anything with tclcad, we need the libged plugins
  add_dependencies(libtclcad ged_plugins)

endif (BRLCAD_ENABLE_TCL)

set(tclcad_ignore_files
  ${LIBTCLCAD_SRCS}
  tclcad_private.h
  view/view.h
  )
CMAKEFILES(${tclcad_ignore_files})
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
