include_directories("${CMAKE_CURRENT_SOURCE_DIR}/..")
BRLCAD_ADDEXEC(simulate_test "simulate_test.cpp" libged TEST)
add_test(NAME simulate_test COMMAND simulate_test)
