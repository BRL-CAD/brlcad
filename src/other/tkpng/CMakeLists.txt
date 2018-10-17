# Minimum required version of CMake
cmake_minimum_required(VERSION 3.1.3)

# set CMake project name
project(TKPNG)

SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/CMake")

find_package(ZLIB)
find_package(TCL)
find_package(X11)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  ${ZLIB_INCLUDE_DIR}
  ${X11_INCLUDE_DIR}
  )

SET(TKPNG_PKGNAME tkpng)
SET(TKPNG_PKGVERSION "0.8")

SET(TKPNG_SRCS
  generic/tkImgPNG.c
  generic/tkImgPNGInit.c
  )

add_library(tkpng SHARED ${TKPNG_SRCS})
target_link_libraries(tkpng ${TCL_LIBRARY} ${TCL_TK_LIBRARY} ${ZLIB_LIBRARY} ${X11_LIBRARIES})
SET_TARGET_PROPERTIES(tkpng PROPERTIES VERSION ${TKPNG_PKGVERSION})
install(TARGETS tkpng 
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})

# Create the pkgIndex.tcl file.
TCL_PKGINDEX(tkpng ${TKPNG_PKGNAME} ${TKPNG_PKGVERSION})
