
# To minimize the number of build targets and binaries that are created, we
# combine most (not all) of the unit tests into a single program.

set(bview_test_srcs
  list.c
  vlist.c
  )

# Generate and assemble the necessary per-test-type source code
set(BVIEW_TEST_SRC_INCLUDES)
set(BVIEW_TEST_SRC_SWITCHES)
set(BVIEW_TEST_LIST)
foreach(srcfile ${bview_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  set(BVIEW_TEST_SRC_INCLUDES "${BVIEW_TEST_SRC_INCLUDES}\n#include \"libbv/tests/${srcfile}\"")
  set(BVIEW_TEST_SRC_SWITCHES "${BVIEW_TEST_SRC_SWITCHES}\n   if (BU_STR_EQUAL(av[0], \"${NAME_ROOT}\")) return ${NAME_ROOT}_main(ac,av);")
  set(BVIEW_TEST_LIST "${BVIEW_TEST_LIST}\\n  ${NAME_ROOT}")
endforeach(srcfile ${bview_test_srcs})

# Generate bview_test.c file and build it
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
configure_file(bview_test.c.in "${CMAKE_CURRENT_BINARY_DIR}/bview_test.c" @ONLY)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/bview_test.c")
BRLCAD_ADDEXEC(bview_test "${CMAKE_CURRENT_BINARY_DIR}/bview_test.c" libbv TEST)

# Housekeeping for included source files
CMAKEFILES(${bview_test_srcs})
VALIDATE_STYLE("${bview_test_srcs}" bview_test)

# This test program is being kept separate because the plot3 logic really
# should move, and when it does it will be easier to move the test if it
# is not tied into the bview_test target
BRLCAD_ADDEXEC(bview_plot3 plot3.c "libbu;libbv" TEST)

#
#  *************** plot3.c ***************
#
#  Tests to detect validity/invalidity of plot3 files
#
BRLCAD_ADD_TEST(NAME bview_plot3_valid COMMAND bview_plot3 -b "${CMAKE_CURRENT_SOURCE_DIR}/valid.plot3")
BRLCAD_ADD_TEST(NAME bview_plot3_invalid COMMAND bview_plot3 -i -b "${CMAKE_CURRENT_SOURCE_DIR}/invalid.plot3")

#
#  ************ list.c tests *************
#
# Format is:
#  COMMAND bview_test list  <2 or 3 (the dimension)>  X0,Y0[,Z0] X1,Y1[,Z1] Xn,Yn[,Zn]
BRLCAD_ADD_TEST(NAME bview_list_2d_null                COMMAND bview_test list  2) # NULL list of points
BRLCAD_ADD_TEST(NAME bview_list_2d_1                   COMMAND bview_test list  2  0,0)
BRLCAD_ADD_TEST(NAME bview_list_2d_2                   COMMAND bview_test list  2  0,0 1.64,1)
BRLCAD_ADD_TEST(NAME bview_list_2d_3                   COMMAND bview_test list  2  3,7 1,-5)
BRLCAD_ADD_TEST(NAME bview_list_2d_4                   COMMAND bview_test list  2  3,7 1,-5.321 67,0 0,100)


BRLCAD_ADD_TEST(NAME bview_list_3d_null                COMMAND bview_test list  3) # NULL list of points
BRLCAD_ADD_TEST(NAME bview_list_3d_1                   COMMAND bview_test list  3  0,0,0)
BRLCAD_ADD_TEST(NAME bview_list_3d_2                   COMMAND bview_test list  3  0,0,3 1.64,1,4)
BRLCAD_ADD_TEST(NAME bview_list_3d_3                   COMMAND bview_test list  3  3,7,8 1,-5,8)
BRLCAD_ADD_TEST(NAME bview_list_3d_4                   COMMAND bview_test list  3  3,7,8 1,-5.321,3 67,0,67 0,100,23)


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
  valid.plot3
  invalid.plot3
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
