include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(ogr_frmt_bna OBJECT ogrbnadriver.cpp ogrbnadatasource.cpp ogrbnalayer.cpp ogrbnaparser.cpp)
