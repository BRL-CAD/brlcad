add_subdirectory(libjson)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/libjson
	)

set(geojson_srcs
	ogrgeojsondriver.cpp
	ogrgeojsondatasource.cpp
	ogrgeojsonlayer.cpp
	ogrgeojsonwritelayer.cpp
	ogrgeojsonutils.cpp
	ogrgeojsonreader.cpp
	ogrgeojsonwriter.cpp
	ogresrijsonreader.cpp
	ogrtopojsonreader.cpp
	)

add_library(ogr_frmt_geojson OBJECT ${geojson_srcs})
