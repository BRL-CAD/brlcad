include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DGDALDRIVER)

add_library(frmt_blx OBJECT blxdataset.cpp blx.c)
