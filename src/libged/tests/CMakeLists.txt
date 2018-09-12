BRLCAD_ADDEXEC(test_tops test_tops.c libged TEST)
add_dependencies(test_tops moss.g)
add_test(NAME test_tops_moss COMMAND test_tops "${CMAKE_BINARY_DIR}/share/db/moss.g")

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

