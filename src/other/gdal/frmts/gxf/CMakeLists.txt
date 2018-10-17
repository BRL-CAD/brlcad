include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(frmt_gxf OBJECT gxfdataset.cpp gxfopen.c gxf_proj4.c gxf_ogcwkt.c)
