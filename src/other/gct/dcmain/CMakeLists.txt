include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
	${CMAKE_CURRENT_SOURCE_DIR}/../../openNURBS
	${CMAKE_CURRENT_SOURCE_DIR}/../../tcl/generic
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/../Auxiliary
)

add_executable(dcmain main.cpp)
target_link_libraries(dcmain librt libbu libbn AUX MeshDecimation ${M_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

