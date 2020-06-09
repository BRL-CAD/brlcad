# Make sure we know about required components
find_package(X11)
find_package(GL)
find_package(Qt5Widgets QUIET)
find_package(TCL)

# Include directories needed by libdm users
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${TCL_INCLUDE_PATH}
  ${TK_INCLUDE_PATH}
  ${Qt5Widgets_INCLUDE_DIRS}
  )

set(QT_LIBRARIES
  ${TCL_LIBRARY}
  ${TK_LIBRARY}
  ${Qt5Widgets_LIBRARIES}
  )

set(QT_SRCS
  dm-qt.cpp
  if_qt.cpp
  )

set_property(SOURCE dm-qt.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)
set_property(SOURCE if_qt.cpp APPEND PROPERTY COMPILE_DEFINITIONS FB_USE_INTERNAL_API)

add_definitions(-DDM_PLUGIN)
add_definitions(-DDM_QT -DIF_QT)

add_library(dm-qt SHARED ${QT_SRCS})
target_link_libraries(dm-qt libdm libbu ${QT_LIBRARIES})
set_property(TARGET dm-qt APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(dm-qt "${QT_SRCS}")

PLUGIN_SETUP(dm-qt dm)

CMAKEFILES(
  fb_qt.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
