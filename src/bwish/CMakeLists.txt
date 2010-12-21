include_directories(
  ${PNG_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
)

IF(WIN32)
	BRLCAD_ADDEXEC(btclsh "cadAppInit.c consoleMain.c" "libtclcad libged")
ELSE(WIN32)
	BRLCAD_ADDEXEC(btclsh "cmd.c input.c main.c tcl.c" "libbu libtclcad libtermio")
ENDIF(WIN32)

IF(BRLCAD-ENABLE_TK)
	IF(WIN32)
		add_executable(bwish WIN32 cadAppInit.c winMain.c)
		target_link_libraries(bwish libtclcad libdm imm32.lib comctl32.lib)
		install(TARGETS bwish RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})
		SET_TARGET_PROPERTIES(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
	ELSE(WIN32)
		BRLCAD_ADDEXEC(bwish "cmd.c input.c main.c tcl.c" "libtclcad libdm libtermio ${X11_LIBRARIES} ${X11_Xi_LIB} libdm")
		SET_TARGET_PROPERTIES(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
	ENDIF(WIN32)
ENDIF(BRLCAD-ENABLE_TK)

IF(ITCL_LIBRARY MATCHES "^itcl$")
   ADD_DEPENDENCIES(btclsh itcl)
ENDIF(ITCL_LIBRARY MATCHES "^itcl$")

IF(ITK_LIBRARY MATCHES "^itk$")
   ADD_DEPENDENCIES(bwish itcl itk)
ENDIF(ITK_LIBRARY MATCHES "^itk$")
