include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_edigeo OBJECT ogredigeodriver.cpp ogredigeodatasource.cpp ogredigeolayer.cpp)
