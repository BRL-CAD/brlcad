if(BRLCAD_ENABLE_BULLET)
  BRLCAD_ADDEXEC(simulate_test "simulate_test.cpp" "libged" TEST)
  add_test(NAME simulate_test COMMAND simulate_test)
endif(BRLCAD_ENABLE_BULLET)

CMAKEFILES(simulate_test.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

