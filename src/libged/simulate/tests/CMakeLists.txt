include_directories("${CMAKE_CURRENT_SOURCE_DIR}/..")


if(BULLET_LIBRARIES)
  BRLCAD_ADDEXEC(simulate_test "simulate_test.cpp" "libged;${BULLET_LIBRARIES}" TEST)
  add_test(NAME simulate_test COMMAND simulate_test)
endif(BULLET_LIBRARIES)

CMAKEFILES(simulate_test.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

