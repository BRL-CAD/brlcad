set(GCT_SAMPLER_SRCS
  PointSampler.cpp
  MarchingCubesSampler.cpp
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../openNURBS
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tcl/generic
  )

add_library(Sampler SHARED ${GCT_SAMPLER_SRCS})
set_target_properties(Sampler PROPERTIES VERSION 1.1.0 SOVERSION 1)

install(TARGETS Sampler
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(MSVC)
  set_property(TARGET Sampler APPEND PROPERTY COMPILE_DEFINITIONS "SAMPLER_DLL_EXPORTS")
endif(MSVC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

