set(BWISH_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${TERMIO_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${DM_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES BWISH_INCLUDE_DIRS)
include_directories(${BWISH_INCLUDE_DIRS})

# Until we figure out how to package require instead
# of using the internal C api, we need the internal
# Tcl headers
include_directories(
  ${CMAKE_SOURCE_DIR}/src/other/tcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/tcl/unix
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itcl/generic
  ${CMAKE_SOURCE_DIR}/src/other/incrTcl/itk/generic
  )

# Which source files constitute btclsh, and which libraries
# it needs, depends on the current OS.
if(WIN32)
  set(btclsh_srcs cadAppInit.c consoleMain.c)
  set(btclsh_libs libtclcad libged ${ITCL_LIBRARY})
else(WIN32)
  set(btclsh_srcs cmd.c input.c main.c tcl.c)
  set(btclsh_libs libtclcad libbu libtermio ${ITCL_LIBRARY})
endif(WIN32)
# Regardless of OS , make sure all the tclsh files are marked for CMake
CMAKEFILES(cmd.c input.c main.c tcl.c cadAppInit.c consoleMain.c)

# Now, define the btclsh target
BRLCAD_ADDEXEC(btclsh "${btclsh_srcs}" "${btclsh_libs}")

# If we have Tk enabled, we want bwish too
if(BRLCAD_ENABLE_TK)
  if(WIN32)
    # Need to add support for specifying WIN32 executable types to
    # BRLCAD_ADDEXEC - in the meantime, do this manually.
    add_executable(bwish WIN32 cadAppInit.c winMain.c)
    target_link_libraries(bwish libtclcad libdm imm32.lib comctl32.lib ${ITCL_LIBRARY} ${ITK_LIBRARY})
    install(TARGETS bwish RUNTIME DESTINATION ${BIN_DIR})
    set_target_properties(bwish PROPERTIES FOLDER "BRL-CAD Executables")
  else(WIN32)
    # If we're not on Windows, bwish is just btclsh with more libraries and
    # an extra compilation definition
    BRLCAD_ADDEXEC(bwish "${btclsh_srcs}" "libtclcad;libdm;libtermio;${X11_LIBRARIES};${X11_Xi_LIB};libdm;${ITCL_LIBRARY};${ITK_LIBRARY}")
  endif(WIN32)
  # Both platform cases get the compilation flag
  set_target_properties(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
endif(BRLCAD_ENABLE_TK)
CMAKEFILES(unixMain.c winMain.c cmd.c input.c main.c tcl.c)

if(ITCL_LIBRARY MATCHES "^itcl$")
  add_dependencies(btclsh itcl)
endif(ITCL_LIBRARY MATCHES "^itcl$")

if(ITK_LIBRARY MATCHES "^itk$" AND BRLCAD_ENABLE_TK)
  add_dependencies(bwish itcl itk)
endif(ITK_LIBRARY MATCHES "^itk$" AND BRLCAD_ENABLE_TK)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
