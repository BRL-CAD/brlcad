set(MGED_INCLUDE_DIRS
  ${TCLCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${DM_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  ${TERMIO_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
LIST(REMOVE_DUPLICATES MGED_INCLUDE_DIRS)
include_directories(${MGED_INCLUDE_DIRS})

# Until we figure out how to properly use package require,
# we need the internal Tcl headers
include_directories(
  ${CMAKE_SOURCE_DIR}/src/other/tcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/tcl/unix
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itk/generic
  )

if(LEX_EXECUTABLE AND YACC_EXECUTABLE)
  add_subdirectory(points)
  set(MGED_EXTRA_LIBS libpoints)
endif(LEX_EXECUTABLE AND YACC_EXECUTABLE)


# process.h in points dir makes Windows unhappy,
# and the TERMLIB_INCLUDE_DIR isn't set there
if(NOT WIN32)
  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/points
    ${TERMLIB_INCLUDE_DIR}
    )
endif(NOT WIN32)

if(WIN32 AND BRLCAD_ENABLE_OPENGL)
  set(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-wgl.c")
endif(WIN32 AND BRLCAD_ENABLE_OPENGL)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)
  set(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-ogl.c")
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)
  set(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-X.c")
endif(BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_TK)

#if(BRLCAD_ENABLE_TK)
#  set(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-tk.c")
#endif(BRLCAD_ENABLE_TK)

if(BRLCAD_ENABLE_RTGL)
  set(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-rtgl.c")
endif(BRLCAD_ENABLE_RTGL)

add_definitions(
  ${MGED_DEFINES}
  ${C99_FLAG}
  )

set(MGED_SOURCES
  adc.c
  animedit.c
  arbs.c
  attach.c
  axes.c
  bodyio.c
  buttons.c
  chgmodel.c
  chgtree.c
  chgview.c
  cmd.c
  clone.c
  color_scheme.c
  columns.c
  dir.c
  ${MGED_DM_SOURCES}
  dm-generic.c
  dm-plot.c
  dm-ps.c
  dodraw.c
  doevent.c
  dozoom.c
  edarb.c
  edars.c
  edpipe.c
  edsol.c
  facedef.c
  fbserv.c
  grid.c
  history.c
  mater.c
  menu.c
  mged.c
  mover.c
  muves.c
  overlay.c
  plot.c
  polyif.c
  predictor.c
  rect.c
  rtif.c
  scroll.c
  set.c
  setup.c
  share.c
  solids_on_ray.c
  tedit.c
  titles.c
  track.c
  update.c
  usepen.c
  utility1.c
  vparse.c
  )
if(NOT WIN32)
  set(MGED_SOURCES ${MGED_SOURCES} vrlink.c)
endif(NOT WIN32)

if(NOT WIN32)
  BRLCAD_ADDEXEC(mged "${MGED_SOURCES}" "libtclcad;libged;libwdb;libdm;libtermio;${MGED_EXTRA_LIBS};${ITCL_LIBRARY};${ITK_LIBRARY}")
else(NOT WIN32)
  BRLCAD_ADDEXEC(mged "${MGED_SOURCES}" "libtclcad;libged;libwdb;libdm;${ITCL_LIBRARY};${ITK_LIBRARY};ws2_32.lib;opengl32.lib")
endif(NOT WIN32)
if(${ITCL_LIBRARY} MATCHES "itcl")
  ADD_DEPENDENCIES(mged itcl)
endif(${ITCL_LIBRARY} MATCHES "itcl")
if(${ITK_LIBRARY} MATCHES "itk")
  ADD_DEPENDENCIES(mged itk)
endif(${ITK_LIBRARY} MATCHES "itk")
if(LEX_EXECUTABLE AND YACC_EXECUTABLE)
  SET_TARGET_PROPERTIES(mged PROPERTIES COMPILE_FLAGS "-DBC_WITH_PARSER")
endif(LEX_EXECUTABLE AND YACC_EXECUTABLE)

# mged depends on the creation/installation of all the
# tclscripts.
foreach(item ${pkgIndex_target_list})
  ADD_DEPENDENCIES(mged ${item})
endforeach(item ${pkgIndex_target_list})
foreach(item ${tclIndex_target_list})
  ADD_DEPENDENCIES(mged ${item})
endforeach(item ${tclIndex_target_list})

BRLCAD_ADDEXEC(cad_boundp cad_boundp.c "libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(cad_parea cad_parea.c libbu)

if(WIN32)
  configure_file(mged.bat ${CMAKE_BINARY_DIR}/bin/mged.bat COPYONLY)
  install(PROGRAMS mged.bat DESTINATION bin)
endif(WIN32)

set(mged_MANS
  cad_boundp.1
  cad_parea.1
  )
ADD_MAN_PAGES(1 mged_MANS)
set(mged_ignore_files
  bool_rewrite.c
  cmd.h
  comb_bool.h
  comb_bool_parse.y
  comb_bool_scan.l
  dm-rtgl.c
  dm-tk.c
  dm-wgl.c
  fbserv.h
  htmlLibraryUi.h
  joints.h
  menu.h
  mgdev.h
  mged.bat
  mged.h
  mged_dm.h
  qray.h
  red.c
  scroll.h
  sedit.h
  titles.h
  )
CMAKEFILES(${mged_ignore_files})
CMAKEFILES(Makefile.am)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
