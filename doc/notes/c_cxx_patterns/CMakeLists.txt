cmake_minimum_required(VERSION 3.12)
project(EXAMPLES)
if(NOT MSVC)
  add_definitions(-g)
  add_definitions(-Wall)
endif(NOT MSVC)

# Example 1 - basic C implementation
add_subdirectory(01_basic_C)

# Example 2 - information hiding C implementation
add_subdirectory(02_hidden_C)

# Example 3 - PImpl C implementation
add_subdirectory(03_PImpl_C)

# Example 4 - information hiding CXX implementation
add_subdirectory(04_hidden_CXX)

# Example 5 - PImpl CXX implementation
add_subdirectory(05_PImpl_CXX)

include(CTest)
enable_testing()
add_test(01_basic_C 01_basic_C/ntest01)
add_test(02_hidden_C 02_hidden_C/ntest02)
add_test(03_PImpl_C 03_PImpl_C/ntest03)
add_test(04_hidden_CXX 04_hidden_CXX/ntest04)
add_test(05_PImpl_CXX 05_PImpl_CXX/ntest05)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

