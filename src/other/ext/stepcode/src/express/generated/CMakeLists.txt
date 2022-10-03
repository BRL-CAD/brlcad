include_directories(.)

add_library(objlib_expscan_c OBJECT expscan.c)
set_property(TARGET objlib_expscan_c PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(objlib_expparse_c OBJECT expparse.c)
set_property(TARGET objlib_expparse_c PROPERTY POSITION_INDEPENDENT_CODE ON)

