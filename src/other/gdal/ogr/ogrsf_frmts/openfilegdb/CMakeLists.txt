include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../mem
	)

set(openfilegdb_srcs
	ogropenfilegdbdriver.cpp
	ogropenfilegdbdatasource.cpp
	ogropenfilegdblayer.cpp
	filegdbtable.cpp
	filegdbindex.cpp
	)

add_library(ogr_frmt_openfilegdb OBJECT ${openfilegdb_srcs})
