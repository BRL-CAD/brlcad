include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(gtm_srcs
	ogrgtmdriver.cpp
	ogrgtmdatasource.cpp
       	ogrgtmlayer.cpp
       	gtm.cpp
       	gtmwaypointlayer.cpp
       	gtmtracklayer.cpp
	)

add_library(ogr_frmt_gtm OBJECT ${gtm_srcs})
