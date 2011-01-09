include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_DIRS}
)

IF(LEX_EXECUTABLE)
	LEX_TARGET(SS_Scanner script.l
		${CMAKE_CURRENT_BINARY_DIR}/script.c COMPILE_FLAGS "-l")
	BRLCAD_ADDEXEC(scriptsort "scriptsort.c ${LEX_SS_Scanner_OUTPUTS}" librt)
ENDIF(LEX_EXECUTABLE)

BRLCAD_ADDEXEC(script-tab script-tab.c "librt libbn libbu")
BRLCAD_ADDEXEC(tabinterp tabinterp.c "librt libbu")
BRLCAD_ADDEXEC(tabsub tabsub.c "libbn libbu")
BRLCAD_ADDEXEC(txyz-pl txyz-pl.c "libbn libbu") 

SET(tab_MANS
  tabinterp.1
  tabsub.1
  scriptsort.1
)
install(FILES ${tab_MANS} DESTINATION ${MAN_DIR}/man1)
