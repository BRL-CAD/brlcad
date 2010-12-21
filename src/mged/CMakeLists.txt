IF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	ADD_SUBDIRECTORY(points)
	SET(pointstarget libpoints)
ENDIF(LEX_EXECUTABLE AND YACC_EXECUTABLE)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TERMLIB_INCLUDE_DIR}
  ${PNG_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR_GL}
  ${OPENGL_INCLUDE_DIR_GLX}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
)

# process.h in points dir makes Windows unhappy
IF(NOT WIN32)
	include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/points
		)
ENDIF(NOT WIN32)

IF(WIN32 AND BRLCAD-ENABLE_OPENGL)
  SET(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-wgl.c")
  SET(MGED_DEFINES "${MGED_DEFINES} -DDM_WGL -DIF_WGL")
ENDIF(WIN32 AND BRLCAD-ENABLE_OPENGL)

IF(BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_OPENGL)
  SET(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-ogl.c")
  SET(MGED_DEFINES "${MGED_DEFINES} -DDM_OGL -DIF_OGL")
ENDIF(BRLCAD-ENABLE_X11 AND BRLCAD-ENABLE_OPENGL)

IF(BRLCAD-ENABLE_X11)
  SET(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-X.c")
  SET(MGED_DEFINES "${MGED_DEFINES} -DDM_X -DIF_X")
ENDIF(BRLCAD-ENABLE_X11)

#IF(BRLCAD-ENABLE_TK)
#  SET(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-tk.c")
#  SET(MGED_DEFINES "${MGED_DEFINES} -DDM_TK -DIF_TK")
#ENDIF(BRLCAD-ENABLE_TK)

#IF(BRLCAD-ENABLE_RTGL)
#  SET(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-rtgl.c")
#  SET(MGED_DEFINES "${MGED_DEFINES} -DDM_RTGL -DIF_RTGL")
#ENDIF(BRLCAD-ENABLE_RTGL)

add_definitions(
  ${MGED_DEFINES}
)

SET(MGED_SOURCES
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
  hideline.c 
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
  qray.c 
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
IF(NOT WIN32)
	SET(MGED_SOURCES ${MGED_SOURCES} vrlink.c)
ENDIF(NOT WIN32)

IF(NOT WIN32)
	BRLCAD_ADDEXEC(mged "${MGED_SOURCES}" "libtclcad libged libwdb libdm libtermio ${pointstarget}")
ELSE(NOT WIN32)
	BRLCAD_ADDEXEC(mged "${MGED_SOURCES}" "libtclcad libged libwdb libdm ws2_32.lib opengl32.lib")
ENDIF(NOT WIN32)

BRLCAD_ADDEXEC(cad_boundp cad_boundp.c libbu)

BRLCAD_ADDEXEC(cad_parea cad_parea.c libbu)

SET(mged_MANS
  cad_boundp.1
  cad_parea.1
)
install(FILES ${mged_MANS} DESTINATION ${MAN_DIR}/man1)
