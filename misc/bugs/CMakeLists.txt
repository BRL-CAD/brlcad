cmake_minimum_required(VERSION 3.20)

project(BrlcadBugs)

enable_testing()

# Define bug tests
add_test(NAME BugTest1 COMMAND ${CMAKE_COMMAND} -E echo "Bug Test 1")
add_test(NAME BugTest2 COMMAND ${CMAKE_COMMAND} -E echo "Bug Test 2")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 cino=N-s
