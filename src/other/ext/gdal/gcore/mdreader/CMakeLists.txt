include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(mdreader_srcs
	reader_digital_globe.cpp
	reader_geo_eye.cpp
	reader_orb_view.cpp
	reader_pleiades.cpp
	reader_rdk1.cpp
	reader_landsat.cpp
	reader_spot.cpp
	reader_rapid_eye.cpp
	reader_alos.cpp
	reader_eros.cpp
	reader_kompsat.cpp
	)

add_library(gdal_mdreader_obj OBJECT ${mdreader_srcs})
set_target_properties(gdal_mdreader_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
