#
# CMakeLists.txt for AP203 Minimum
#
#  This file is released to the public domain.  Any part of this file may be
# freely copied in part or in full for any purpose.  No acknowledgment is required
# for the use of this file.
#
project(AP203Minimum)
cmake_minimum_required(VERSION 3.6.3)

INCLUDE( ExternalProject )

set(CMAKE_MODULE_PATH ${AP203Minimum_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

INCLUDE( External_STEPCode )

#####
# Variables ideally set by FindSTEPCode.cmake

IF(NOT WIN32)
	set( STEPCODE_LIBRARIES
	${STEPCODE_INSTALL_DIR}/lib/libbase.a
	${STEPCODE_INSTALL_DIR}/lib/libstepcore.a
	${STEPCODE_INSTALL_DIR}/lib/libstepeditor.a
	${STEPCODE_INSTALL_DIR}/lib/libstepdai.a
	${STEPCODE_INSTALL_DIR}/lib/libsteputils.a
	${STEPCODE_INSTALL_DIR}/lib/libsdai_ap203.a
	)
ELSE()
	set( STEPCODE_LIBRARIES
	${STEPCODE_INSTALL_DIR}/lib/libbase.lib
	${STEPCODE_INSTALL_DIR}/lib/libstepcore.lib
	${STEPCODE_INSTALL_DIR}/lib/libstepeditor.lib
	${STEPCODE_INSTALL_DIR}/lib/libstepdai.lib
	${STEPCODE_INSTALL_DIR}/lib/libsteputils.lib
	${STEPCODE_INSTALL_DIR}/lib/libsdai_ap203.lib
	${STEPCODE_INSTALL_DIR}/lib/libexpress.lib
	${STEPCODE_INSTALL_DIR}/lib/libexppp.lib
	shlwapi.lib
	)
ENDIF()

MESSAGE( STATUS "STEPCODE_INSTALL_DIR: " ${STEPCODE_INSTALL_DIR} )

set( STEPCODE_INCLUDE_DIR
${STEPCODE_INSTALL_DIR}/include/stepcode
${STEPCODE_INSTALL_DIR}/include/stepcode/base
${STEPCODE_INSTALL_DIR}/include/stepcode/clstepcore
${STEPCODE_INSTALL_DIR}/include/stepcode/cldai
${STEPCODE_INSTALL_DIR}/include/stepcode/clutils
${STEPCODE_INSTALL_DIR}/include/stepcode/cleditor
${STEPCODE_INSTALL_DIR}/include/schemas/sdai_ap203
)

# End of variables ideally set by FindSTEPCode.cmake
######


include_directories(
    ${STEPCODE_INCLUDE_DIR}
)

set(SRCS ../ap203min.cpp)

set(HDRS )

add_executable( ${PROJECT_NAME} ${SRCS} ${HDRS})

add_dependencies( ${PROJECT_NAME} STEPCODE )

target_link_libraries( ${PROJECT_NAME} 
    ${STEPCODE_LIBRARIES}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
