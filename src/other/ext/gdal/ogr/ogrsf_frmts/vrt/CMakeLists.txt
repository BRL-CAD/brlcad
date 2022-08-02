include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../generic
	)

add_library(ogr_frmt_vrt OBJECT ogrvrtdatasource.cpp ogrvrtlayer.cpp ogrvrtdriver.cpp)
