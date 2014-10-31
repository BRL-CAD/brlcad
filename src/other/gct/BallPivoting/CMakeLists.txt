include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../Auxiliary
  )

add_library(BallPivoting SHARED ballpivoting.c)
target_link_libraries(BallPivoting AUX)
set_target_properties(BallPivoting PROPERTIES VERSION 1.0.0 SOVERSION 1)

install(TARGETS BallPivoting
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(MSVC)
  set_property(TARGET BallPivoting APPEND PROPERTY COMPILE_DEFINITIONS "BALLPIVOTING_DLL_EXPORTS")
endif(MSVC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
