# Include directories needed by libpkg users
SET(PKG_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	)
BRLCAD_INCLUDE_DIRS(PKG_INCLUDE_DIRS)

# Local includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

BRLCAD_ADD_DEF_LISTS(BU_DEFINES PKG_DEFINES)

add_executable(pkg_test_server server.c)
target_link_libraries(pkg_test_server libpkg libbu)

add_executable(pkg_test_client client.c)
target_link_libraries(pkg_test_client libpkg libbu)

CMAKEFILES(ntp.h)
