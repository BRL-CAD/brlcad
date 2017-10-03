include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../vrt
	${CMAKE_CURRENT_SOURCE_DIR}/../raw
	${CMAKE_CURRENT_SOURCE_DIR}/../../ogr/ogrsf_frmts/geojson
	${CMAKE_CURRENT_SOURCE_DIR}/../../ogr/ogrsf_frmts/geojson/libjson
	)

set(pds_srcs
	pdsdataset.cpp
	isis2dataset.cpp
	isis3dataset.cpp
	vicardataset.cpp
	nasakeywordhandler.cpp
	vicarkeywordhandler.cpp
	)

add_library(frmt_pds OBJECT ${pds_srcs})
