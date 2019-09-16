include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../xplane
	)
add_library(ogr_frmt_sua OBJECT ogrsuadriver.cpp ogrsuadatasource.cpp ogrsualayer.cpp)
