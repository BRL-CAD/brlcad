include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	)

add_library(ogr_frmt_mem OBJECT ogrmemdriver.cpp ogrmemdatasource.cpp ogrmemlayer.cpp)
