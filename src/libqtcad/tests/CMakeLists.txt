set(QG_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/include
  ${QTCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(QG_DIRS)

set(QGMODEL_SRCS qgmodel.cpp)
set(QGVIEW_SRCS qgview.cpp)

if (TARGET Qt6::Test)
  add_executable(qgmodel qgmodel.cpp)
  target_link_libraries(qgmodel libqtcad libged librt libbu Qt6::Widgets Qt6::Test)
endif (TARGET Qt6::Test)
if (TARGET Qt5::Test)
  add_executable(qgmodel ${QGMODEL_SRCS})
  target_link_libraries(qgmodel libqtcad libged librt libbu Qt5::Widgets Qt5::Test)
endif (TARGET Qt5::Test)
if (TARGET qgmodel)
  target_compile_definitions(qgmodel PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qgmodel
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (TARGET qgmodel)

if (TARGET Qt6::Widgets)
  add_executable(qgview ${QGVIEW_SRCS})
  target_link_libraries(qgview libqtcad libged librt libbu Qt6::Widgets)
endif (TARGET Qt6::Widgets)
if (TARGET Qt5::Widgets)
  add_executable(qgview ${QGVIEW_SRCS})
  target_link_libraries(qgview libqtcad libged librt libbu Qt5::Widgets)
endif (TARGET Qt5::Widgets)
if (TARGET qgview)
  target_compile_definitions(qgview PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qgview
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (TARGET qgview)

CMAKEFILES(
  CMakeLists.txt
  ${QGMODEL_SRCS}
  ${QGVIEW_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

