include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(ogr_frmt_arcgen OBJECT ograrcgendriver.cpp ograrcgendatasource.cpp ograrcgenlayer.cpp)
