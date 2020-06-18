include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../generic
	)

add_definitions(-DSAOffset=vsi_l_offset)
add_definitions(-DUSE_CPL)

set(shape_srcs
	shape2ogr.cpp
	shpopen.c
	dbfopen.c
	shptree.c
	sbnsearch.c
	shp_vsi.c
	ogrshapedriver.cpp
	ogrshapedatasource.cpp
	ogrshapelayer.cpp
	)

add_library(ogr_frmt_shape OBJECT ${shape_srcs})
