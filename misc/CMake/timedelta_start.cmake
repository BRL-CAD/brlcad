# Write out entries to populate a tm struct (will be used for time
# delta calculations later)

set(DELTA_START "${CMAKE_BINARY_DIR}/CMakeTmp/DELTA_START")
configure_file("${CMAKE_SOURCE_DIR}/misc/CMake/test_srcs/timedelta_start.c.in" "${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_start.c")
try_run(TIME_RESULT TIME_COMPILED "${CMAKE_BINARY_DIR}/CMakeTmp" "${CMAKE_BINARY_DIR}/CMakeTmp/timedelta_start.c" OUTPUT_VARIABLE COMPILEMESSAGES)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

