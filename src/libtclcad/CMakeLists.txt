# Include directories needed by libtclcad users
set(TCLCAD_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${DM_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_DIRS}
  )

# Include directories only needed by the implementation
set(TCLCAD_LOCAL_INCLUDE_DIRS
  ${PNG_INCLUDE_DIR}
  )

BRLCAD_LIB_INCLUDE_DIRS(tclcad TCLCAD_INCLUDE_DIRS TCLCAD_LOCAL_INCLUDE_DIRS)

if(BRLCAD_ENABLE_X11)
  set(TCLCAD_XLIBS ${TCLCAD_XLIBS} ${X11_LIBRARIES} ${X11_Xi_LIB})
endif(BRLCAD_ENABLE_X11)

set(LIBTCLCAD_SRCS
  libfuncs.c
  tclcad.c
  tclcadAutoPath.c
  tclcad_obj.c
  tclcad_eval.c
  tclcad_init.c
  tkImgFmtPIX.c
  cmdhist_obj.c
  )
set_property(SOURCE tclcad_obj.c APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
BRLCAD_ADDLIB(libtclcad "${LIBTCLCAD_SRCS}" "libged;libdm;libfb;libbn;libbu;${TCL_LIBRARY};${TK_LIBRARY};${ITCL_LIBRARY};${ITK_LIBRARY};${TCLCAD_XLIBS}")
SET_TARGET_PROPERTIES(libtclcad PROPERTIES VERSION 20.0.1 SOVERSION 20)
set(tclcad_ignore_files tclcad_private.h)
CMAKEFILES(${tclcad_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
