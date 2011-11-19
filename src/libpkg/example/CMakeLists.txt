# Include directories needed by libpkg users
SET(PKG_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${BU_INCLUDE_DIRS}
	)
BRLCAD_INCLUDE_DIRS(PKG_INCLUDE_DIRS)

# Local includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(pkg_test_server server.c)
target_link_libraries(pkg_test_server libpkg libbu)
SET_TARGET_PROPERTIES(pkg_test_server PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")

add_executable(pkg_test_client client.c)
target_link_libraries(pkg_test_client libpkg libbu)
SET_TARGET_PROPERTIES(pkg_test_client PROPERTIES COMPILE_FLAGS "-DBRLCAD_DLL")

CMAKEFILES(ntp.h)
