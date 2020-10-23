include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${ZLIB_INCLUDE_DIRS}
	)
add_library(frmt_ozi OBJECT ozidataset.cpp)
if (TARGET "${ZLIB_LIBRARY}")
	add_dependencies(frmt_ozi ${ZLIB_LIBRARY})
endif (TARGET "${ZLIB_LIBRARY}")

