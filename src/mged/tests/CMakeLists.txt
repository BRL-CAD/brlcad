# Headless unit tests for MGED's cooperative interrupt policy (Increment 1).
# The pure decision helpers in ../interrupt.c carry no MGED/Tcl/GED state, so
# they compile directly into a small self-contained test executable.

brlcad_addexec(mged_test_interrupt "mged_test_interrupt.cpp;../interrupt.c" libbu TEST)
target_include_directories(mged_test_interrupt BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

brlcad_add_test(NAME mged_test_interrupt COMMAND mged_test_interrupt)

cmakefiles(
  CMakeLists.txt
  mged_test_interrupt.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
