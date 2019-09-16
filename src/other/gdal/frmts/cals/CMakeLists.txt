include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../gtiff/libtiff
	)
add_library(frmt_cals OBJECT calsdataset.cpp)
