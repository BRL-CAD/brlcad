include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../mem
	)
add_library(frmt_xpm OBJECT xpmdataset.cpp)
