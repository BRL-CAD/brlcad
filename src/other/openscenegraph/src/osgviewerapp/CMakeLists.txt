include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/../src/OpenThreads/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/OpenThreads/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

add_executable(osgviewer osgviewer.cpp)
target_link_libraries(osgviewer osgViewer ${OPENGL_gl_LIBRARY})
