# This should get rolled into a macro as more formats are added

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dem/dem-regress.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/dem/dem-regress.cmake @ONLY)
ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dem/hawaii-c.done
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/dem/dem-regress.cmake
	DEPENDS dem-g ${CMAKE_CURRENT_SOURCE_DIR}/dem/hawaii-c.tar.bz2 ${CMAKE_CURRENT_SOURCE_DIR}/dem/hawaii-c-control.tar.bz2 
	)

ADD_CUSTOM_TARGET(dem-regress DEPENDS dem-g ${CMAKE_CURRENT_BINARY_DIR}/dem/hawaii-c.done)

CMAKEFILES(dem)
