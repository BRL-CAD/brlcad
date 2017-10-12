include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../../frmts/iso8211
	)

set(s57_srcs
	ogrs57driver.cpp
	ogrs57datasource.cpp
	ogrs57layer.cpp
	s57reader.cpp
	s57writer.cpp
	ddfrecordindex.cpp
	s57classregistrar.cpp
	s57filecollector.cpp
	s57featuredefns.cpp
	)

add_library(ogr_frmt_s57 OBJECT ${s57_srcs})
