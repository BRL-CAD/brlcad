include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(nlib05 SHARED nlib.cxx)
add_executable(ntest05 main.c)
target_link_libraries(ntest05 nlib05)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

