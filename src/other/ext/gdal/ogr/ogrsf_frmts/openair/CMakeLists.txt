include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../xplane
	)

set(openair_srcs
	ogropenairdriver.cpp
	ogropenairdatasource.cpp
	ogropenairlayer.cpp
	ogropenairlabellayer.cpp
	)

add_library(ogr_frmt_openair OBJECT ${openair_srcs})
