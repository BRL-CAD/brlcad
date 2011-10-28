SET(MGED_INCLUDE_DIRS
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

IF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	ADD_SUBDIRECTORY(points)
	SET(MGED_EXTRA_LIBS libpoints)
ENDIF(LEX_EXECUTABLE AND YACC_EXECUTABLE)


# process.h in points dir makes Windows unhappy,
# and the TERMLIB_INCLUDE_DIR isn't set there
IF(NOT WIN32)
	include_directories(
		${CMAKE_CURRENT_SOURCE_DIR}/points
		${TERMLIB_INCLUDE_DIR}
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

IF(BRLCAD-ENABLE_RTGL)
  SET(MGED_DM_SOURCES "${MGED_DM_SOURCES};dm-rtgl.c")
  SET(MGED_DEFINES "${MGED_DEFINES} -DDM_RTGL -DIF_RTGL")
ENDIF(BRLCAD-ENABLE_RTGL)

add_definitions(
  ${MGED_DEFINES}
  ${C99_FLAG}
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
	BRLCAD_ADDEXEC(mged "${MGED_SOURCES}" "libtclcad libged libwdb libdm libtermio ${MGED_EXTRA_LIBS} ${ITCL_LIBRARY} ${ITK_LIBRARY}")
ELSE(NOT WIN32)
	BRLCAD_ADDEXEC(mged "${MGED_SOURCES}" "libtclcad libged libwdb libdm ${ITCL_LIBRARY} ${ITK_LIBRARY} ws2_32.lib opengl32.lib")
ENDIF(NOT WIN32)
IF(${ITCL_LIBRARY} MATCHES "itcl")
	ADD_DEPENDENCIES(mged itcl)
ENDIF(${ITCL_LIBRARY} MATCHES "itcl")
IF(${ITK_LIBRARY} MATCHES "itk")
	ADD_DEPENDENCIES(mged itk)
ENDIF(${ITK_LIBRARY} MATCHES "itk")
IF(LEX_EXECUTABLE AND YACC_EXECUTABLE)
	SET_TARGET_PROPERTIES(mged PROPERTIES COMPILE_FLAGS "-DBC_WITH_PARSER")
ENDIF(LEX_EXECUTABLE AND YACC_EXECUTABLE)

BRLCAD_ADDEXEC(cad_boundp cad_boundp.c "libbu ${M_LIBRARY}")

BRLCAD_ADDEXEC(cad_parea cad_parea.c libbu)

IF(WIN32)
	configure_file(mged.bat ${CMAKE_BINARY_DIR}/bin/mged.bat COPYONLY)
	INSTALL(PROGRAMS mged.bat DESTINATION bin)
ENDIF(WIN32)

SET(mged_MANS
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
