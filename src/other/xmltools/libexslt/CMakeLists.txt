
SET(VERSION "0.8.15")
SET(LIBEXSLT_VERSION_NUMBER 815)
SET(LIBEXSLT_VERSION_EXTRA "-BRLCAD")
SET(WITH_CRYPTO 0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/exsltconfig.h.in
	${CMAKE_CURRENT_BINARY_DIR}/../include/libexslt/exsltconfig.h)

SET(libexslt_srcs
	src/common.c
	src/crypto.c
	src/date.c
	src/dynamic.c
	src/exslt.c
	src/functions.c
	src/math.c
	src/saxon.c
	src/sets.c
	src/strings.c
	)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}/../include/libxslt
	${CMAKE_CURRENT_BINARY_DIR}/../include/libexslt
	${CMAKE_CURRENT_BINARY_DIR}/../include
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/../libxslt/include/libxslt
	${CMAKE_CURRENT_SOURCE_DIR}/../libxslt/include
	${CMAKE_CURRENT_SOURCE_DIR}/include/libexslt
	${CMAKE_CURRENT_SOURCE_DIR}/../libxml/include
	)

add_library(exslt SHARED ${libexslt_srcs})
target_link_libraries(exslt xml xslt)
