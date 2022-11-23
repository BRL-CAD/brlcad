include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_wasp OBJECT ogrwaspdriver.cpp ogrwaspdatasource.cpp ogrwasplayer.cpp)
