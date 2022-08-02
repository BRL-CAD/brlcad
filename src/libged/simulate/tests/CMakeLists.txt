if(BRLCAD_ENABLE_BULLET)
  include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../bullet)
  BRLCAD_ADDEXEC(simulate_test "simulate_test.cpp" "libged" TEST)
  BRLCAD_ADD_TEST(NAME simulate_test COMMAND simulate_test)
endif(BRLCAD_ENABLE_BULLET)

CMAKEFILES(simulate_test.cpp)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

