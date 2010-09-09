include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${BRLCAD_STD_INCLUDES}
)
add_definitions(
  ${BRLCAD_STD_DEFS}
)

LEX_TARGET(SS_Scanner script.l
    ${CMAKE_CURRENT_BINARY_DIR}/script.c COMPILE_FLAGS "-l")

BRLCAD_ADDEXEC(scriptsort "scriptsort.c ${LEX_SS_Scanner_OUTPUTS}" librt)
BRLCAD_ADDEXEC(script-tab script-tab.c "bu bn librt")
BRLCAD_ADDEXEC(tabinterp tabinterp.c "bu librt")
BRLCAD_ADDEXEC(tabsub tabsub.c "bu bn")
BRLCAD_ADDEXEC(txyz-pl txyz-pl.c "bu bn") 

SET(tab_MANS
  tabinterp.1
  tabsub.1
  scriptsort.1
)
install(FILES ${tab_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
