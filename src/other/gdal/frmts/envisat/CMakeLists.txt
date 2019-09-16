include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../raw
	)
add_library(frmt_envisat OBJECT EnvisatFile.c records.c adsrange.cpp unwrapgcps.cpp envisatdataset.cpp)
