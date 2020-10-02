include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../ogr/ogrsf_frmts/avc
	${CMAKE_CURRENT_SOURCE_DIR}/../../ogr/ogrsf_frmts/shape
	)
add_library(frmt_aigrid OBJECT aigdataset.cpp gridlib.c aigopen.c aigccitt.c)
