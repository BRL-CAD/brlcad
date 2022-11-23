include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_htf OBJECT ogrhtfdriver.cpp ogrhtfdatasource.cpp ogrhtflayer.cpp)
