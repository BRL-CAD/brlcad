include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(frmt_usgsdem OBJECT usgsdemdataset.cpp usgsdem_create.cpp)
