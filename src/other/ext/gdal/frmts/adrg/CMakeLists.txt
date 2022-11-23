include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../iso8211
	)
add_library(frmt_adrg OBJECT adrgdataset.cpp srpdataset.cpp)
