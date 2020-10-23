include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../raw
	)
add_library(frmt_ceos2 OBJECT sar_ceosdataset.cpp ceosrecipe.c ceossar.c ceos.c link.c)
