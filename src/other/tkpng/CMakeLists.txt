include_directories(
		  ${CMAKE_CURRENT_SOURCE_DIR}/src
		  ${CMAKE_CURRENT_BINARY_DIR}
		  ${TCL_INCLUDE_PATH}
		  ${TK_INCLUDE_PATH}
		  ${X11_INCLUDE_DIR}
		  )

SET(TKPNG_PKGNAME tkpng)
SET(TKPNG_PKGVERSION "0.8")

SET(TKPNG_SRCS
		  generic/tkImgPNG.c
		  generic/tkImgPNGInit.c
)

add_library(tkpng SHARED ${TKPNG_SRCS})
install(TARGETS tkpng DESTINATION lib)

# Create the pkgIndex.tcl file.
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${TKPNG_PKGNAME} ${TKPNG_PKGVERSION} [list load [file join $dir .. libtkpng${CMAKE_SHARED_LIBRARY_SUFFIX}] tkpng]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${TKPNG_PKGNAME}${TKPNG_PKGVERSION})

