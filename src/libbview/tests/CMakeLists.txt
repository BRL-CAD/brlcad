
# To minimize the number of build targets and binaries that are created, we
# combine most (not all) of the unit tests into a single program.

set(bview_test_srcs
  vlist.c
  )

# Generate and assemble the necessary per-test-type source code
set(BVIEW_TEST_SRC_INCLUDES)
set(BVIEW_TEST_SRC_SWITCHES)
set(BVIEW_TEST_LIST)
foreach(srcfile ${bview_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  set(BVIEW_TEST_SRC_INCLUDES "${BVIEW_TEST_SRC_INCLUDES}\n#include \"libbview/tests/${srcfile}\"")
  set(BVIEW_TEST_SRC_SWITCHES "${BVIEW_TEST_SRC_SWITCHES}\n   if (BU_STR_EQUAL(av[0], \"${NAME_ROOT}\")) return ${NAME_ROOT}_main(ac,av);")
  set(BVIEW_TEST_LIST "${BVIEW_TEST_LIST}\\n  ${NAME_ROOT}")
endforeach(srcfile ${bview_test_srcs})

# Generate bview_test.c file and build it
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
configure_file(bview_test.c.in "${CMAKE_CURRENT_BINARY_DIR}/bview_test.c" @ONLY)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/bview_test.c")
BRLCAD_ADDEXEC(bview_test "${CMAKE_CURRENT_BINARY_DIR}/bview_test.c" libbview TEST)

# Housekeeping for included source files
CMAKEFILES(${bview_test_srcs})
VALIDATE_STYLE("${bview_test_srcs}" bview_test)

#
#  *************** vlist.c ***************
#
# For vlist_cnt, input is as follows:
# vlist_cnt <args>

# the <args> format is as follows:
# expected_result
#
BRLCAD_ADD_TEST(NAME bview_vlist_cmd_cnt_NULL COMMAND bview_test vlist -1)
BRLCAD_ADD_TEST(NAME bview_vlist_cmd_cnt_0 COMMAND bview_test vlist 0)
BRLCAD_ADD_TEST(NAME bview_vlist_cmd_cnt_1 COMMAND bview_test vlist 5)
BRLCAD_ADD_TEST(NAME bview_vlist_cmd_cnt_15 COMMAND bview_test vlist 15)
BRLCAD_ADD_TEST(NAME bview_vlist_cmd_cnt_45 COMMAND bview_test vlist 45)
BRLCAD_ADD_TEST(NAME bview_vlist_cmd_cnt_500 COMMAND bview_test vlist 500)


CMAKEFILES(
  CMakeLists.txt
  bview_test.c.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
