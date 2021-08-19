include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/include
  ${QTCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  )

if (BRLCAD_ENABLE_QT)

  add_executable(qgmodel qgmodel.cpp)
  target_link_libraries(qgmodel libqtcad libged librt libbu Qt5::Widgets)
  target_compile_definitions(qgmodel PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qgmodel
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})

endif (BRLCAD_ENABLE_QT)


CMAKEFILES(
  CMakeLists.txt
  qgmodel.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

