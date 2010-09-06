include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
)

add_definitions(
  ${BRLCAD_STD_DEFS}
)


IF(BRLCAD-ENABLE_X11)
  BRLCAD_ADDEXEC(ssampview "spectrum.c ssampview.c" "optical fb ${TK_LIBRARY} ${X11_LIBRARIES}")
ENDIF(BRLCAD-ENABLE_X11)

add_executable(rttherm spectrum.c viewtherm.c)
TARGET_LINK_LIBRARIES(rttherm librttherm optical fb multispectral ged)
SET_TARGET_PROPERTIES(rttherm PROPERTIES COMPILE_DEFINITIONS "RT_MULTISPECTRAL")
INSTALL(PROGRAMS ${BRLCAD_BINARY_DIR}/bin/rttherm DESTINATION
  ${BRLCAD_INSTALL_BIN_DIR})

BRLCAD_ADDEXEC(ssamp-bw ssamp-bw.c "optical")

set(rttherm_MANS
  rttherm.1
  ssamp-bw.1
)
install(FILES ${rttherm_MANS} DESTINATION ${BRLCAD_INSTALL_MAN_DIR}/man1)
