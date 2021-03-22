include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	)

set(xplane_srcs
	ogrxplanedriver.cpp
	ogrxplanedatasource.cpp
	ogrxplanelayer.cpp
	ogr_xplane_geo_utils.cpp
	ogr_xplane_reader.cpp
	ogr_xplane_apt_reader.cpp
	ogr_xplane_nav_reader.cpp
	ogr_xplane_fix_reader.cpp
	ogr_xplane_awy_reader.cpp
	)

add_library(ogr_frmt_xplane OBJECT ${xplane_srcs})
