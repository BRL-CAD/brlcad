include_directories(
  ${PNG_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  ${TCL_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/other/tcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/tcl/unix
  ${TK_INCLUDE_PATH}
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itk/generic
)

IF(WIN32)
	BRLCAD_ADDEXEC(btclsh "cadAppInit.c consoleMain.c" "libtclcad libged ${ITCL_LIBRARY}")
	CMAKEFILES(cmd.c input.c main.c tcl.c)
ELSE(WIN32)
	BRLCAD_ADDEXEC(btclsh "cmd.c input.c main.c tcl.c" "libbu libtclcad libtermio ${ITCL_LIBRARY}")
	CMAKEFILES(cadAppInit.c consoleMain.c)
ENDIF(WIN32)

IF(BRLCAD-ENABLE_TK)
	IF(WIN32)
		add_executable(bwish WIN32 cadAppInit.c winMain.c)
		target_link_libraries(bwish libtclcad libdm imm32.lib comctl32.lib ${ITCL_LIBRARY} ${ITK_LIBRARY})
		CMAKEFILES(cmd.c input.c main.c tcl.c)
	ELSE(WIN32)
		BRLCAD_ADDEXEC(bwish "cmd.c input.c main.c tcl.c" "libtclcad libdm libtermio ${X11_LIBRARIES} ${X11_Xi_LIB} libdm ${ITCL_LIBRARY} ${ITK_LIBRARY}")
		CMAKEFILES(winMain.c)
	ENDIF(WIN32)
	SET_TARGET_PROPERTIES(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
	install(TARGETS bwish RUNTIME DESTINATION ${BIN_DIR})
ENDIF(BRLCAD-ENABLE_TK)

CMAKEFILES(unixMain.c)

IF(ITCL_LIBRARY MATCHES "^itcl$")
   ADD_DEPENDENCIES(btclsh itcl)
ENDIF(ITCL_LIBRARY MATCHES "^itcl$")

IF(ITK_LIBRARY MATCHES "^itk$" AND BRLCAD-ENABLE_TK)
   ADD_DEPENDENCIES(bwish itcl itk)
ENDIF(ITK_LIBRARY MATCHES "^itk$" AND BRLCAD-ENABLE_TK)
