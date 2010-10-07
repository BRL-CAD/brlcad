include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_PATH}
)

LEX_TARGET(SS_Scanner script.l
    ${CMAKE_CURRENT_BINARY_DIR}/script.c COMPILE_FLAGS "-l")

BRLCAD_ADDEXEC(scriptsort "scriptsort.c ${LEX_SS_Scanner_OUTPUTS}" librt)
BRLCAD_ADDEXEC(script-tab script-tab.c "libbu libbn librt")
BRLCAD_ADDEXEC(tabinterp tabinterp.c "libbu librt")
BRLCAD_ADDEXEC(tabsub tabsub.c "libbu libbn")
BRLCAD_ADDEXEC(txyz-pl txyz-pl.c "libbu libbn") 

SET(tab_MANS
  tabinterp.1
  tabsub.1
  scriptsort.1
)
install(FILES ${tab_MANS} DESTINATION ${MAN_DIR}/man1)
