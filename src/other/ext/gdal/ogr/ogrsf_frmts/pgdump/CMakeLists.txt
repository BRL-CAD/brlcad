include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_pgdump OBJECT ogrpgdumpdriver.cpp ogrpgdumpdatasource.cpp ogrpgdumplayer.cpp)
