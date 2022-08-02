include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../gtiff/libtiff
	${CMAKE_CURRENT_SOURCE_DIR}/../gtiff
	${CMAKE_CURRENT_SOURCE_DIR}/../vrt
	)

set(nitf_srcs
	nitfdataset.cpp
	rpftocdataset.cpp
	nitfwritejpeg.cpp
	nitfwritejpeg_12.cpp
	nitfrasterband.cpp
	ecrgtocdataset.cpp
	nitffile.c
	nitfimage.c
	mgrs.c
	nitfaridpcm.cpp
	nitfbilevel.cpp
	rpftocfile.cpp
	nitfdes.c
	nitf_gcprpc.cpp
	)

add_library(frmt_nitf OBJECT ${nitf_srcs})
