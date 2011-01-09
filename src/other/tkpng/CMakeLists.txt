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
install(TARGETS tkpng DESTINATION lib)

# Create the pkgIndex.tcl file.
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${TKPNG_PKGNAME} ${TKPNG_PKGVERSION} [list load [file join $dir .. libtkpng${CMAKE_SHARED_LIBRARY_SUFFIX}] tkpng]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${TKPNG_PKGNAME}${TKPNG_PKGVERSION})

