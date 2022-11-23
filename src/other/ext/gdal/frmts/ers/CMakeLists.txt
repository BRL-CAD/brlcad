include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../raw
	)
add_library(frmt_ers OBJECT ersdataset.cpp ershdrnode.cpp)
