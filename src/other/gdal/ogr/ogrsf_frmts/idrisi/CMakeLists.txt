include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../../frmts
	)
add_library(ogr_frmt_idrisi OBJECT ogridrisidriver.cpp ogridrisidatasource.cpp ogridrisilayer.cpp)
