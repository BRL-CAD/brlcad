include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../ogr/ogrsf_frmts/geojson
	${CMAKE_CURRENT_SOURCE_DIR}/../../ogr/ogrsf_frmts/geojson/libjson
	${CMAKE_CURRENT_SOURCE_DIR}/../raw
	)
add_library(frmt_arg OBJECT argdataset.cpp)
