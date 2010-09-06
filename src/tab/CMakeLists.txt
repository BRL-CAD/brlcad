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

SET(tab_MANS
  scriptsort.1
)
#are tabinterp.1 and tabsub.1 still needed?  Makefile.am didn't build these
#binaries
install(FILES ${tab_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
