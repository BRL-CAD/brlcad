# To minimize the number of build targets and binaries that are created, we
# combine some of the unit tests into a single program.

set(nmg_test_srcs
  mk.c
  copy.c
  )

# Generate and assemble the necessary per-test-type source code
set(NMG_TEST_SRC_INCLUDES)
set(NMG_TEST_SRC_SWITCHES)
set(NMG_TEST_LIST)
foreach(srcfile ${nmg_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  set(NMG_TEST_SRC_INCLUDES "${NMG_TEST_SRC_INCLUDES}\n#define main ${NAME_ROOT}_main\n#include \"libnmg/tests/${srcfile}\"\n#undef main\n")
  set(NMG_TEST_SRC_SWITCHES "${NMG_TEST_SRC_SWITCHES}\n   if (BU_STR_EQUAL(av[0], \"${NAME_ROOT}\")) return ${NAME_ROOT}_main(ac,av);")
  set(NMG_TEST_LIST "${NMG_TEST_LIST}\\n  ${NAME_ROOT}")
endforeach(srcfile ${nmg_test_srcs})

# Generate nmg_test.cpp file and build it
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
configure_file(nmg_test.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/nmg_test.cpp" @ONLY)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/nmg_test.cpp")
BRLCAD_ADDEXEC(nmg_test "${CMAKE_CURRENT_BINARY_DIR}/nmg_test.cpp" libnmg TEST)


# nmg_mk testing
BRLCAD_ADD_TEST(NAME nmg_mk COMMAND nmg_test mk)

# nmg_copy testing
BRLCAD_ADD_TEST(NAME nmg_copy COMMAND nmg_test copy)

CMAKEFILES(
  CMakeLists.txt
  ${nmg_test_srcs}
  nmg_test.cpp.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
