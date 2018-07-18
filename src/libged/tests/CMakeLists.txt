BRLCAD_ADDEXEC(test_tops test_tops.c libged TEST)
add_dependencies(test_tops moss.g)
add_test(NAME test_tops_moss COMMAND test_tops "${CMAKE_BINARY_DIR}/share/db/moss.g")
