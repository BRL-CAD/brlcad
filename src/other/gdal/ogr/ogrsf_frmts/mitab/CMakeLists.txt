include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	)

set(mitab_srcs
	mitab_rawbinblock.cpp
	mitab_mapheaderblock.cpp
	mitab_mapindexblock.cpp
	mitab_mapobjectblock.cpp
	mitab_mapcoordblock.cpp
	mitab_feature.cpp
	mitab_feature_mif.cpp
	mitab_mapfile.cpp
	mitab_idfile.cpp
	mitab_datfile.cpp
	mitab_tabfile.cpp
	mitab_miffile.cpp
	mitab_utils.cpp
	mitab_imapinfofile.cpp
	mitab_middatafile.cpp
	mitab_bounds.cpp
	mitab_maptoolblock.cpp
	mitab_tooldef.cpp
	mitab_coordsys.cpp
	mitab_spatialref.cpp
	mitab_ogr_driver.cpp
	mitab_indfile.cpp
	mitab_tabview.cpp
	mitab_ogr_datasource.cpp
	mitab_geometry.cpp
	mitab_tabseamless.cpp
	)

add_definitions(-DOGR)
add_definitions(-DMITAB_USE_OFTDATETIME)

add_library(ogr_frmt_mitab OBJECT ${mitab_srcs})
