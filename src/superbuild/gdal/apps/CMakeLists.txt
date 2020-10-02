
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../frmts/vrt
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/geojson/libjson
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/geojson
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/generic
	)

set(apps_srcs
	commonutils.cpp
	gdalinfo_lib.cpp
	gdal_translate_lib.cpp
	gdalwarp_lib.cpp
	ogr2ogr_lib.cpp
	gdaldem_lib.cpp
	nearblack_lib.cpp
	gdal_grid_lib.cpp
	gdal_rasterize_lib.cpp
	gdalbuildvrt_lib.cpp
	)

add_library(gdal_apps_obj OBJECT ${apps_srcs})
set_target_properties(gdal_apps_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
