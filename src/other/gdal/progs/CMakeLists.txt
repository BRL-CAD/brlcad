include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../apps
	${CMAKE_CURRENT_SOURCE_DIR}/../frmts/vrt
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/geojson/libjson
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/geojson
	${CMAKE_CURRENT_SOURCE_DIR}/../ogr/ogrsf_frmts/generic
	)

add_executable(gdalinfo gdalinfo_bin.cpp)
target_link_libraries(gdalinfo gdal)
set_target_properties(gdalinfo PROPERTIES FOLDER "Third Party Executables/GDAL")
install(TARGETS gdalinfo DESTINATION ${BIN_DIR})

add_executable(gdallocationinfo gdallocationinfo.cpp)
target_link_libraries(gdallocationinfo gdal)
set_target_properties(gdallocationinfo PROPERTIES FOLDER "Third Party Executables/GDAL")
install(TARGETS gdallocationinfo DESTINATION ${BIN_DIR})

add_executable(gdal_translate gdal_translate_bin.cpp)
target_link_libraries(gdal_translate gdal)
set_target_properties(gdal_translate PROPERTIES FOLDER "Third Party Executables/GDAL")
install(TARGETS gdal_translate DESTINATION ${BIN_DIR})

add_executable(gdaltransform gdaltransform.cpp)
target_link_libraries(gdaltransform gdal)
set_target_properties(gdaltransform PROPERTIES FOLDER "Third Party Executables/GDAL")
install(TARGETS gdaltransform DESTINATION ${BIN_DIR})

add_executable(gdaldem gdaldem_bin.cpp)
target_link_libraries(gdaldem gdal)
set_target_properties(gdaldem PROPERTIES FOLDER "Third Party Executables/GDAL")
install(TARGETS gdaldem DESTINATION ${BIN_DIR})

add_executable(gdalwarp gdalwarp_bin.cpp)
target_link_libraries(gdalwarp gdal)
set_target_properties(gdalwarp PROPERTIES FOLDER "Third Party Executables/GDAL")
install(TARGETS gdalwarp DESTINATION ${BIN_DIR})

add_executable(gdalbuildvrt gdalbuildvrt_bin.cpp)
target_link_libraries(gdalbuildvrt gdal)
set_target_properties(gdalbuildvrt PROPERTIES FOLDER "Third Party Executables/GDAL")
install(TARGETS gdalbuildvrt DESTINATION ${BIN_DIR})

