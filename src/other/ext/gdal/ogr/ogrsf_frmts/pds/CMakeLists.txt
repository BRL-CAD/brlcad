include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../../frmts/pds
	)
add_library(ogr_frmt_pds OBJECT ogrpdsdriver.cpp ogrpdsdatasource.cpp ogrpdslayer.cpp)
