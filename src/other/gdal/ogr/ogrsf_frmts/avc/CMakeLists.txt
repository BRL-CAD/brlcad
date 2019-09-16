include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../shape
	)

set(avc_srcs
	avc_bin.c
	avc_binwr.c
	avc_e00gen.c
	avc_e00parse.c
	avc_e00write.c
	avc_e00read.c
	avc_mbyte.c
	avc_misc.c
	avc_rawbin.c
	ogravcbindriver.cpp
	ogravcbindatasource.cpp
	ogravcbinlayer.cpp
	ogravclayer.cpp
	ogravcdatasource.cpp
	ogravce00layer.cpp
	ogravce00datasource.cpp
	ogravce00driver.cpp
	)

add_library(ogr_frmt_avc OBJECT ${avc_srcs})
