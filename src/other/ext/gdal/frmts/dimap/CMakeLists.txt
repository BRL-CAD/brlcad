include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../vrt
	)
add_library(frmt_dimap OBJECT dimapdataset.cpp)
