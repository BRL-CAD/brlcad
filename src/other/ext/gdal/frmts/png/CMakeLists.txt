include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${ZLIB_INCLUDE_DIRS}
	${PNG_INCLUDE_DIRS}
	)
add_library(frmt_png OBJECT pngdataset.cpp)
if (TARGET png)
	add_dependencies(frmt_png png)
endif (TARGET png)
