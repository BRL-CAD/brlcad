include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_segy OBJECT ogrsegydriver.cpp ogrsegydatasource.cpp ogrsegylayer.cpp)
