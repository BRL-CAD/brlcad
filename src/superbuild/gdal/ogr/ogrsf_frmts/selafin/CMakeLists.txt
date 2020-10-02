include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_selafin OBJECT ogrselafindriver.cpp ogrselafindatasource.cpp ogrselafinlayer.cpp io_selafin.cpp)
