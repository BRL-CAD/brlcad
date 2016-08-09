include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
	${CMAKE_CURRENT_SOURCE_DIR}/../../openNURBS
	${CMAKE_CURRENT_SOURCE_DIR}/../../tcl/generic
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/../Auxiliary
)

add_executable(psmain main.cpp)
target_link_libraries(psmain librt libbu libbn AUX Sampler BrlcadGCT ${CMAKE_THREAD_LIBS_INIT})

