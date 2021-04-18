include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_sxf OBJECT ogrsxfdriver.cpp ogrsxfdatasource.cpp ogrsxflayer.cpp)
