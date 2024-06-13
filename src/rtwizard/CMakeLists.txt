if (TARGET libtclcad)

  set(RTWIZARD_SRCS main.c)

  if (HAVE_WINDOWS_H)
    # To associate an icon with the application for Windows (needed
    # for the WiX installer) we must use an rc file.
    enable_language(RC)
    set(RTWIZARD_SRCS ${RTWIZARD_SRCS} rtwizard.rc)
  endif (HAVE_WINDOWS_H)


  BRLCAD_ADDEXEC(rtwizard "${RTWIZARD_SRCS}" "libtclcad;librt;libdm;liboptical;libicv;Threads::Threads;${M_LIBRARY}")
  target_include_directories(rtwizard BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  ADD_TARGET_DEPS(rtwizard rt rtedge fbserv fb-png fb-pix fbclear fblabel fbfree fbline fb-fb pix-bw bwmod bw-pix pixmatte pix-fb tkpng TclIndexBld dm_plugins)

  # rtwizard needs the tclIndex and pkgIndex files set up correctly
  add_dependencies(rtwizard TclIndexBld)

  # RtWizard is one of the programs that gets a start menu entry
  set_property(INSTALL "${BIN_DIR}/$<TARGET_FILE_NAME:rtwizard>"
    PROPERTY CPACK_START_MENU_SHORTCUTS "RtWizard ${BRLCAD_VERSION}"
    )
  set_property(INSTALL "${BIN_DIR}/$<TARGET_FILE_NAME:rtwizard>"
    PROPERTY CPACK_DESKTOP_SHORTCUTS "RtWizard ${BRLCAD_VERSION}"
    )

endif (TARGET libtclcad)

CMAKEFILES(
  CMakeLists.txt
  main.c
  rtwizard.rc
  rtwizard.ico
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
