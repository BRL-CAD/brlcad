include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	)

set(libjson_srcs
	arraylist.c
	debug.c
	json_object.c
	json_tokener.c
	json_util.c
	linkhash.c
	printbuf.c
	json_object_iterator.c
	json_c_version.c
	)

if(NOT APPLE)
  add_definitions(-D_XOPEN_SOURCE=500)
endif(NOT APPLE)

add_library(json_obj OBJECT ${libjson_srcs})
set_target_properties(json_obj PROPERTIES FOLDER "Third Party Libraries/GDAL")
