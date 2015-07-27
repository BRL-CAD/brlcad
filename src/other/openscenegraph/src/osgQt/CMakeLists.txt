if(DYNAMIC_OPENSCENEGRAPH)
  add_definitions(-DOSGQT_LIBRARY)
else()
  add_definitions(-DOSG_LIBRARY_STATIC)
endif()

QT5_WRAP_CPP( SOURCES_H_MOC ${SOURCES_H} OPTIONS "-fosgQt/QGraphicsViewAdapter" )

# FIXME: For OS X, need flag for Framework or dylib
SET(OSGQT_SRCS
  GraphicsWindowQt.cpp
  QFontImplementation.cpp
  QGraphicsViewAdapter.cpp
  QWidgetImage.cpp
  ${SOURCES_H_MOC}
  ${OPENSCENEGRAPH_VERSIONINFO_RC}
  )

if(CMAKE_COMPILER_IS_GNUCXX)
  # Remove -pedantic flag as it barfs on Qt headers
  STRING(REGEX REPLACE "-pedantic" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

add_library(osgQt ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${OSGQT_SRCS})
target_link_libraries(osgQt osgWidget osgDB osgUtil osg OpenThreads ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY})
install(TARGETS osgQt
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

qt5_use_modules(osgQt Widgets OpenGL)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

