include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_gmt OBJECT ogrgmtdriver.cpp ogrgmtdatasource.cpp ogrgmtlayer.cpp)
