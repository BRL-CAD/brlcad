include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(frmt_dted OBJECT dted_api.c dteddataset.cpp dted_create.c dted_ptstream.c)
