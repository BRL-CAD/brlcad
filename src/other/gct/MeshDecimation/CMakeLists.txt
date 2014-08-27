set(GCT_DECIMATION_SRCS
	meshcorrection.c
	meshdecimation.c
	meshdecimationSSE2.c
	meshdecimationSSE3.c
	meshdecimationSSE4p1.c
	meshoptimizer.c
	meshoptimization.cpp
	)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../Auxiliary
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../openNURBS
  ${CMAKE_CURRENT_SOURCE_DIR}/../../tcl/generic
  )

add_library(MeshDecimation SHARED ${GCT_DECIMATION_SRCS})
target_link_libraries(MeshDecimation AUX ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(MeshDecimation PROPERTIES VERSION 1.1.0 SOVERSION 1)

install(TARGETS MeshDecimation
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(MSVC)
  set_property(TARGET MeshDecimation APPEND PROPERTY COMPILE_DEFINITIONS "DECIMATION_DLL_EXPORTS")
endif(MSVC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

