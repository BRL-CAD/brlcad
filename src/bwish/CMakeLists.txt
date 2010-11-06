include_directories(
  ${PNG_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
)

IF(BRLCAD-ENABLE_TK)
   BRLCAD_ADDEXEC(bwish "cmd.c input.c main.c tcl.c" "libtclcad libdm libtermio ${X11_LIBRARIES} ${X11_Xi_LIB} libdm")
   SET_TARGET_PROPERTIES(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
ENDIF(BRLCAD-ENABLE_TK)

BRLCAD_ADDEXEC(btclsh "cmd.c input.c main.c tcl.c" "libbu libtclcad libtermio")
