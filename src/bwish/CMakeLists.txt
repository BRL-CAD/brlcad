include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_PNG_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
)

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD=1
)


IF(TK_LIBRARY)
   BRLCAD_ADDEXEC(bwish "cmd.c input.c main.c tcl.c" "tclcad dm termio")
   SET_TARGET_PROPERTIES(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
ENDIF(TK_LIBRARY)

BRLCAD_ADDEXEC(btclsh "cmd.c input.c main.c tcl.c" "bu tclcad termio ${X11_LIBRARIES} ${X11_Xi_LIB} dm")
