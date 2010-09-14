include_directories(
  ${PNG_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
)

IF(TK_LIBRARY)
   BRLCAD_ADDEXEC(bwish "cmd.c input.c main.c tcl.c" "libtclcad libdm libtermio")
   SET_TARGET_PROPERTIES(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
ENDIF(TK_LIBRARY)

BRLCAD_ADDEXEC(btclsh "cmd.c input.c main.c tcl.c" "libbu libtclcad libtermio ${X11_LIBRARIES} ${X11_Xi_LIB} libdm")
