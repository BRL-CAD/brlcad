set(ads_SRCS
  ads_globals.cpp
  DockAreaTabBar.cpp
  DockAreaTitleBar.cpp
  DockAreaWidget.cpp
  DockContainerWidget.cpp
  DockManager.cpp
  DockOverlay.cpp
  DockSplitter.cpp
  DockWidget.cpp
  DockWidgetTab.cpp
  DockingStateReader.cpp
  DockFocusController.cpp
  ElidingLabel.cpp
  FloatingDockContainer.cpp
  FloatingDragPreview.cpp
  IconProvider.cpp
  DockComponentsFactory.cpp
  ads.qrc
  )

set(ads_HEADERS
  ads_globals.h
  DockAreaTabBar.h
  DockAreaTitleBar.h
  DockAreaTitleBar_p.h
  DockAreaWidget.h
  DockContainerWidget.h
  DockManager.h
  DockOverlay.h
  DockSplitter.h
  DockWidget.h
  DockWidgetTab.h
  DockingStateReader.h
  DockFocusController.h
  ElidingLabel.h
  FloatingDockContainer.h
  FloatingDragPreview.h
  IconProvider.h
  DockComponentsFactory.h
  )

if (BRLCAD_ENABLE_QT)
find_package(Qt5 5.5 COMPONENTS Core Gui Widgets REQUIRED)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (UNIX)
  set(ads_SRCS linux/FloatingWidgetTitleBar.cpp ${ads_SRCS})
  set(ads_HEADERS linux/FloatingWidgetTitleBar.h ${ads_HEADERS})
endif()

add_library(qtadvanceddocking SHARED ${ads_SRCS} ${ads_HEADERS})
target_compile_definitions(qtadvanceddocking PRIVATE ADS_SHARED_EXPORT)
target_link_libraries(qtadvanceddocking PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets)
set_target_properties(qtadvanceddocking PROPERTIES 
  AUTOMOC ON
  AUTORCC ON
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  VERSION 1
  )
install(TARGETS qtadvanceddocking
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )
endif (BRLCAD_ENABLE_QT)

CMAKEFILES(
  ${ads_SRCS}
  ${ads_HEADERS}
  CMakeLists.txt
  LICENSE
  README.md
  images/close-button-disabled.svg
  images/close-button-focused.svg
  images/close-button.svg
  images/detach-button-disabled.svg
  images/detach-button.svg
  images/tabs-menu-button.svg
  linux/FloatingWidgetTitleBar.cpp
  linux/FloatingWidgetTitleBar.h
  stylesheets/default.css
  stylesheets/default_linux.css
  stylesheets/focus_highlighting.css
  stylesheets/focus_highlighting_linux.css
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
