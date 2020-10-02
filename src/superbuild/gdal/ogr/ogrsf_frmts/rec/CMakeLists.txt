include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_rec OBJECT ogrrecdriver.cpp ogrrecdatasource.cpp ogrreclayer.cpp ll_recio.cpp)
