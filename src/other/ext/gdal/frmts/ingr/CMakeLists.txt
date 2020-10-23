include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../gtiff/libtiff
	${CMAKE_CURRENT_SOURCE_DIR}/../gtiff
	)
add_library(frmt_ingr OBJECT IntergraphDataset.cpp IntergraphBand.cpp IngrTypes.cpp JpegHelper.cpp)
