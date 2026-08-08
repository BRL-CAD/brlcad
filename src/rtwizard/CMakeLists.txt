set(RTWIZARD_SRCS main.c animation.cpp render.cpp)

set(RTWIZARD_HAVE_QT 0)
if(BRLCAD_ENABLE_QT AND TARGET libqtcad)
  find_package_qt(REQUIRED)
  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(RTWIZARD_MOC_SRCS gui.h)
  else()
    qt5_wrap_cpp(RTWIZARD_MOC_SRCS gui.h)
  endif()
  list(APPEND RTWIZARD_SRCS gui.cpp ${RTWIZARD_MOC_SRCS})
  set(RTWIZARD_HAVE_QT 1)
endif()

if(HAVE_WINDOWS_H)
  # To associate an icon with the application for Windows (needed
  # for the WiX installer) we must use an rc file.
  enable_language(RC)
  set(RTWIZARD_SRCS ${RTWIZARD_SRCS} rtwizard.rc)
endif(HAVE_WINDOWS_H)

brlcad_addexec(rtwizard "${RTWIZARD_SRCS}" "libged;librt;libbv;libdm;libpkg;liboptical;libicv;Threads::Threads;${M_LIBRARY}")
target_include_directories(rtwizard BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(rtwizard PRIVATE RTWIZARD_HAVE_GUI=${RTWIZARD_HAVE_QT})
if(RTWIZARD_HAVE_QT)
  if(Qt6Widgets_FOUND)
    target_link_libraries(rtwizard libqtcad Qt6::Widgets)
  else()
    target_link_libraries(rtwizard libqtcad Qt5::Widgets)
  endif()
endif()
add_target_deps(rtwizard rt rtedge fbserv dm_plugins ged-draw ged-view ged-get_eyemodel)

# RtWizard is one of the programs that gets a start menu entry
set_property(
  INSTALL "${BIN_DIR}/$<TARGET_FILE_NAME:rtwizard>"
  PROPERTY CPACK_START_MENU_SHORTCUTS "RtWizard ${BRLCAD_VERSION}"
)
set_property(
  INSTALL "${BIN_DIR}/$<TARGET_FILE_NAME:rtwizard>"
  PROPERTY CPACK_DESKTOP_SHORTCUTS "RtWizard ${BRLCAD_VERSION}"
)

cmakefiles(
  CMakeLists.txt
  animation.cpp
  animation.h
  gui.cpp
  gui.h
  main.c
  README.md
  render.cpp
  render.h
  rtwizard.rc
  settings.h
  rtwizard.ico
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
