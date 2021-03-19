add_subdirectory(libtiff)
add_subdirectory(libgeotiff)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/libtiff
	${CMAKE_CURRENT_SOURCE_DIR}/libgeotiff
	)

set(gtiff_srcs
	geotiff.cpp
	gt_wkt_srs.cpp
	gt_citation.cpp
	gt_overview.cpp
	tif_float.c
	tifvsi.cpp
	gt_jpeg_copy.cpp
	)

add_definitions(-DINTERNAL_LIBGEOTIFF)
add_definitions(-DINTERNAL_LIBTIFF)
add_definitions(-DBIGTIFF_SUPPORT)

add_library(frmt_gtiff OBJECT ${gtiff_srcs})
