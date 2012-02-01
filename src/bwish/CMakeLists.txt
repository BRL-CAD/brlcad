set(BWISH_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${TERMIO_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${DM_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES BWISH_INCLUDE_DIRS)
include_directories(${BWISH_INCLUDE_DIRS})

# Until we figure out how to package require instead
# of using the interal C api, we need the internal
# Tcl headers
include_directories(
  ${CMAKE_SOURCE_DIR}/src/other/tcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/tcl/unix
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itk/generic
  )

if(WIN32)
  BRLCAD_ADDEXEC(btclsh "cadAppInit.c consoleMain.c" "libtclcad libged ${ITCL_LIBRARY}")
  CMAKEFILES(cmd.c input.c main.c tcl.c)
else(WIN32)
  BRLCAD_ADDEXEC(btclsh "cmd.c input.c main.c tcl.c" "libbu libtclcad libtermio ${ITCL_LIBRARY}")
  CMAKEFILES(cadAppInit.c consoleMain.c)
endif(WIN32)

if(BRLCAD_ENABLE_TK)
  if(WIN32)
    add_executable(bwish WIN32 cadAppInit.c winMain.c)
    target_link_libraries(bwish libtclcad libdm imm32.lib comctl32.lib ${ITCL_LIBRARY} ${ITK_LIBRARY})
    CMAKEFILES(cmd.c input.c main.c tcl.c)
  else(WIN32)
    BRLCAD_ADDEXEC(bwish "cmd.c input.c main.c tcl.c" "libtclcad libdm libtermio ${X11_LIBRARIES} ${X11_Xi_LIB} libdm ${ITCL_LIBRARY} ${ITK_LIBRARY}")
    CMAKEFILES(winMain.c)
  endif(WIN32)
  SET_TARGET_PROPERTIES(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
  install(TARGETS bwish RUNTIME DESTINATION ${BIN_DIR})
endif(BRLCAD_ENABLE_TK)

CMAKEFILES(unixMain.c)

if(ITCL_LIBRARY MATCHES "^itcl$")
  ADD_DEPENDENCIES(btclsh itcl)
endif(ITCL_LIBRARY MATCHES "^itcl$")

if(ITK_LIBRARY MATCHES "^itk$" AND BRLCAD_ENABLE_TK)
  ADD_DEPENDENCIES(bwish itcl itk)
endif(ITK_LIBRARY MATCHES "^itk$" AND BRLCAD_ENABLE_TK)

CMAKEFILES(Makefile.am)
