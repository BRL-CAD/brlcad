include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(nlib01 SHARED nlib.c)
add_executable(ntest01 main.c)
target_link_libraries(ntest01 nlib01)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

