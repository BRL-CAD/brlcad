include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../libtiff
	)

set(libgeotiff_srcs
	xtiff.c
	geo_free.c
	geo_get.c
	geo_names.c
	geo_new.c
	geo_print.c
	geo_set.c
	geo_tiffp.c
	geo_write.c
	geo_normalize.c
	geotiff_proj4.c
	geo_extra.c
	geo_trans.c
	geo_simpletags.c
	)

add_definitions(-DDONT_DEPRECATE_SPRINTF)

add_library(libgeotiff_obj OBJECT ${libgeotiff_srcs})
set_target_properties(libgeotiff_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
