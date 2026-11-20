set(RTWIZARD_SRCS main.c render.cpp)

if(HAVE_WINDOWS_H)
  # To associate an icon with the application for Windows (needed
  # for the WiX installer) we must use an rc file.
  enable_language(RC)
  list(APPEND RTWIZARD_SRCS rtwizard.rc)
endif(HAVE_WINDOWS_H)

set(RTWIZARD_LIBS libged librt libbv libdm libpkg libicv Threads::Threads ${M_LIBRARY})
set(RTWIZARD_TARGET_DEPS rt rtedge fbserv dm_plugins ged-draw ged-view ged-get_eyemodel)
set(RTWIZARD_HAVE_TCL 0)
if(TARGET libtclcad)
  list(APPEND RTWIZARD_LIBS libtclcad liboptical)
  list(
    APPEND
    RTWIZARD_TARGET_DEPS
    tkpng
    TclIndexBld
  )
  set(RTWIZARD_HAVE_TCL 1)
endif(TARGET libtclcad)

brlcad_addexec(rtwizard "${RTWIZARD_SRCS}" "${RTWIZARD_LIBS}")
target_include_directories(rtwizard BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(rtwizard PRIVATE RTWIZARD_HAVE_TCL=${RTWIZARD_HAVE_TCL})
add_target_deps(rtwizard ${RTWIZARD_TARGET_DEPS})

if(RTWIZARD_HAVE_TCL)
  add_dependencies(rtwizard TclIndexBld)

  # RtWizard is one of the GUI programs that gets a start menu entry
  set_property(
    INSTALL "${BIN_DIR}/$<TARGET_FILE_NAME:rtwizard>"
    PROPERTY CPACK_START_MENU_SHORTCUTS "RtWizard ${BRLCAD_VERSION}"
  )
  set_property(
    INSTALL "${BIN_DIR}/$<TARGET_FILE_NAME:rtwizard>"
    PROPERTY CPACK_DESKTOP_SHORTCUTS "RtWizard ${BRLCAD_VERSION}"
  )
endif(RTWIZARD_HAVE_TCL)

cmakefiles(
  CMakeLists.txt
  main.c
  render.cpp
  render.h
  settings.h
  rtwizard.rc
  rtwizard.ico
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
