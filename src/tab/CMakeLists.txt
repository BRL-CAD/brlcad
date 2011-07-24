include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_DIRS}
)

IF(LEX_EXECUTABLE)
	LEX_TARGET(SS_Scanner script.l
		${CMAKE_CURRENT_BINARY_DIR}/script.c COMPILE_FLAGS "-l")
	BRLCAD_ADDEXEC(scriptsort "scriptsort.c ${LEX_SS_Scanner_OUTPUTS}" librt)
	SET_TARGET_PROPERTIES(scriptsort PROPERTIES COMPILE_FLAGS "-Wno-error")
ENDIF(LEX_EXECUTABLE)

BRLCAD_ADDEXEC(script-tab script-tab.c "librt libbn libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(tabinterp tabinterp.c "librt libbu ${M_LIBRARY}")
BRLCAD_ADDEXEC(tabsub tabsub.c "libbn libbu ${M_LIBRARY}")

SET(tab_MANS
  tabinterp.1
  tabsub.1
  scriptsort.1
)
ADD_MAN_PAGES(1 tab_MANS)
CMAKEFILES(script.l tokens.h)
