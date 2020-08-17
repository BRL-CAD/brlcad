include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../xplane
	)

add_library(ogr_frmt_aeronavfaa OBJECT ograeronavfaadriver.cpp ograeronavfaadatasource.cpp ograeronavfaalayer.cpp)
