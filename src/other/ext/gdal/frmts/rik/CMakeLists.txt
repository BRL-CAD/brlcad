include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	${ZLIB_INCLUDE_DIRS}
	)
add_library(frmt_rik OBJECT rikdataset.cpp)
if (TARGET "${ZLIB_LIBRARY}")
	add_dependencies(frmt_rik ${ZLIB_LIBRARY})
endif (TARGET "${ZLIB_LIBRARY}")

