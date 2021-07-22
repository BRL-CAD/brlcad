
# To minimize the number of build targets and binaries that are created, we
# combine most (not all) of the unit tests into a single program.

set(bn_test_srcs
  complex.c
  list.c
  mat.c
  plane_dist.c
  plane_isect.c
  plane_pt.c
  poly_add.c
  poly_multiply.c
  poly_scale.c
  poly_sub.c
  poly_synthetic_div.c
  qmath.c
  sobolseq.c
  tabdata.c
  vlist.c
  )

# Generate and assemble the necessary per-test-type source code
set(BN_TEST_SRC_INCLUDES)
set(BN_TEST_SRC_SWITCHES)
set(BN_TEST_LIST)
foreach(srcfile ${bn_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  set(BN_TEST_SRC_INCLUDES "${BN_TEST_SRC_INCLUDES}\n#include \"libbn/tests/${srcfile}\"")
  set(BN_TEST_SRC_SWITCHES "${BN_TEST_SRC_SWITCHES}\n   if (BU_STR_EQUAL(av[0], \"${NAME_ROOT}\")) return ${NAME_ROOT}_main(ac,av);")
  set(BN_TEST_LIST "${BN_TEST_LIST}\\n  ${NAME_ROOT}")
endforeach(srcfile ${bn_test_srcs})

# Generate bn_test.c file and bnild it
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
configure_file(bn_test.c.in "${CMAKE_CURRENT_BINARY_DIR}/bn_test.c" @ONLY)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/bn_test.c")
BRLCAD_ADDEXEC(bn_test "${CMAKE_CURRENT_BINARY_DIR}/bn_test.c" libbn TEST)

# Housekeeping for included source files
CMAKEFILES(${bn_test_srcs})
VALIDATE_STYLE("${bn_test_srcs}" bn_test)


# TODO - convert this into the scheme used by other tests below
BRLCAD_ADDEXEC(bntester bntester.c "libbu;libbn" TEST)

# WIP - needs to become real test
BRLCAD_ADDEXEC(bn_randsph randsph.c "libbu;libbn" TEST)

# This test program is being kept separate because the plot3 logic really
# should move, and when it does it will be easier to move the test if it
# is not tied into the bn_test target
BRLCAD_ADDEXEC(bn_plot3 plot3.c "libbu;libbn" TEST)

#
#  ************ list.c tests *************
#
# Format is:
#  COMMAND bn_test list  <2 or 3 (the dimension)>  X0,Y0[,Z0] X1,Y1[,Z1] Xn,Yn[,Zn]
BRLCAD_ADD_TEST(NAME bn_list_2d_null                COMMAND bn_test list  2) # NULL list of points
BRLCAD_ADD_TEST(NAME bn_list_2d_1                   COMMAND bn_test list  2  0,0)
BRLCAD_ADD_TEST(NAME bn_list_2d_2                   COMMAND bn_test list  2  0,0 1.64,1)
BRLCAD_ADD_TEST(NAME bn_list_2d_3                   COMMAND bn_test list  2  3,7 1,-5)
BRLCAD_ADD_TEST(NAME bn_list_2d_4                   COMMAND bn_test list  2  3,7 1,-5.321 67,0 0,100)


BRLCAD_ADD_TEST(NAME bn_list_3d_null                COMMAND bn_test list  3) # NULL list of points
BRLCAD_ADD_TEST(NAME bn_list_3d_1                   COMMAND bn_test list  3  0,0,0)
BRLCAD_ADD_TEST(NAME bn_list_3d_2                   COMMAND bn_test list  3  0,0,3 1.64,1,4)
BRLCAD_ADD_TEST(NAME bn_list_3d_3                   COMMAND bn_test list  3  3,7,8 1,-5,8)
BRLCAD_ADD_TEST(NAME bn_list_3d_4                   COMMAND bn_test list  3  3,7,8 1,-5.321,3 67,0,67 0,100,23)


#
#  *********** poly.c tests *************
#
#Format is:
# COMMAND bn_test poly      <  void      >         void
BRLCAD_ADD_TEST(NAME bn_poly_multiply              COMMAND bn_test poly_multiply)
BRLCAD_ADD_TEST(NAME bn_poly_scale                 COMMAND bn_test poly_scale)
BRLCAD_ADD_TEST(NAME bn_poly_add                   COMMAND bn_test poly_add)

# failure encountered, no output to indicate why (disabled in prep for release)
# BRLCAD_ADD_TEST(NAME bn_poly_sub                   COMMAND bn_test poly_sub)

BRLCAD_ADD_TEST(NAME bn_poly_synthetic_div               COMMAND bn_test poly_synthetic_div)


#
#  ************ plane.c tests *************

# For COMMAND bn_test plane_pt, the input format is as follows:
#
# COMMAND bn_test plane_pt <function number> <args>

# For function #1 (bn_3pts_collinear), the <args> format is as follows:
#
# P0 P1 P2 <expected result>
#
# where P0-P2 are the points in question. An individual point is three
# integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_false_1        COMMAND bn_test plane_pt 1  0,0,0 1,1,1 6,3,8 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_false_2        COMMAND bn_test plane_pt 1 0,0,0 6,3,8 1,1,1 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_false_3        COMMAND bn_test plane_pt 1 1,1,1 0,0,0 6,3,8 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_false_4        COMMAND bn_test plane_pt 1 1,1,1 6,3,8 0,0,0 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_false_5        COMMAND bn_test plane_pt 1 6,3,8 1,1,1 0,0,0  0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_false_6        COMMAND bn_test plane_pt 1 6,3,8 0,0,0 1,1,1 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_same           COMMAND bn_test plane_pt 1 5.3,8.1,9.6 5.3,8.1,9.6 5.3,8.1,9.6 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_partial_same_1 COMMAND bn_test plane_pt 1 6,2,4 6,2,4 9.3,8.1,4.6 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_partial_same_2 COMMAND bn_test plane_pt 1 9.3,8.1,4.6 6,2,4 6,2,4 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_partial_same_3 COMMAND bn_test plane_pt 1 6,2,4 9.3,8.1,4.6 6,2,4 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_true_1         COMMAND bn_test plane_pt 1 1.8,4.6,2.2 5.4,13.8,6.6 16.2,41.4,19.8 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_true_2         COMMAND bn_test plane_pt 1 1.8,4.6,2.2 16.2,41.4,19.8 5.4,13.8,6.6 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_true_3         COMMAND bn_test plane_pt 1 5.4,13.8,6.6 1.8,4.6,2.2 16.2,41.4,19.8 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_true_4         COMMAND bn_test plane_pt 1 5.4,13.8,6.6 16.2,41.4,19.8 1.8,4.6,2.2 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_true_5         COMMAND bn_test plane_pt 1 16.2,41.4,19.8 1.8,4.6,2.2 5.4,13.8,6.6 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_collinear_true_6         COMMAND bn_test plane_pt 1 16.2,41.4,19.8 5.4,13.8,6.6 1.8,4.6,2.2 1)

# For function #2 (bn_3pts_distinct), the <args> format is as follows:
#
#  P0 P1 P2 <expected result>
#
# where P0-P2 are the points in question. An individual point
# is three integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_false_all       COMMAND bn_test plane_pt 2 5.3,8.6,9.1 5.3,8.6,9.1 5.3,8.6,9.1 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_false_1         COMMAND bn_test plane_pt 2 5.3,8.6,9.1 5.3,8.6,9.1 8.7,4.1,9.3 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_false_2         COMMAND bn_test plane_pt 2 5.3,8.6,9.1 8.7,4.1,9.3 5.3,8.6,9.1 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_false_3         COMMAND bn_test plane_pt 2 8.7,4.1,9.3 5.3,8.6,9.1 5.3,8.6,9.1 0)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_true_1          COMMAND bn_test plane_pt 2 8.7,4.1,9.3 7.5,9.2,4.1 5.3,8.6,9.1 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_true_2          COMMAND bn_test plane_pt 2 8.7,4.1,9.3 5.3,8.6,9.1 7.5,9.2,4.1 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_true_3          COMMAND bn_test plane_pt 2 5.3,8.6,9.1 8.7,4.1,9.3 7.5,9.2,4.1 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_true_4          COMMAND bn_test plane_pt 2 5.3,8.6,9.1 7.5,9.2,4.1 8.7,4.1,9.3 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_true_5          COMMAND bn_test plane_pt 2 7.5,9.2,4.1 8.7,4.1,9.3 5.3,8.6,9.1 1)
BRLCAD_ADD_TEST(NAME bn_plane_3pts_distinct_true_6          COMMAND bn_test plane_pt 2 7.5,9.2,4.1 5.3,8.6,9.1 8.7,4.1,9.3 1)

# For function #3 (bn_distsq_line3_pt3), the <args> format is as follows:
#
#  PT DIR A <expected result>
#
# Where PT and DIR define the parametric line in question and A is the
# point in question.

BRLCAD_ADD_TEST(NAME bn_plane_distsq_line3_pt3_1            COMMAND bn_test plane_pt 3 0,0,0 1,1,1 0,0,0 0)
BRLCAD_ADD_TEST(NAME bn_plane_distsq_line3_pt3_2            COMMAND bn_test plane_pt 3 -1,-1,-1 1,1,1 0,0,0 0)
BRLCAD_ADD_TEST(NAME bn_plane_distsq_line3_pt3_3            COMMAND bn_test plane_pt 3 0,0,0 1,1,1 0,5,0 16.666666)
BRLCAD_ADD_TEST(NAME bn_plane_distsq_line3_pt3_4            COMMAND bn_test plane_pt 3 5,6,4 9,3,8 -5,2,4 48.441558)

# For function #4 (bn_distsq_pt3_lseg3_v2), the <args> format is as follows:
#
#  A B P <expected return> <expected distance>
#
# Where A and B are the endpoints of the line segment, and P is the
# point in question.

BRLCAD_ADD_TEST(NAME bn_plane_distsq_pt3_lseg3_v2_case0_1   COMMAND bn_test plane_pt 4 -5,-5,-5 5,5,5 0,0,0 0 0)
BRLCAD_ADD_TEST(NAME bn_plane_distsq_pt3_lseg3_v2_case1_1   COMMAND bn_test plane_pt 4 -5,-5,-5 5,5,5 -5,-5,-5 1 0)
BRLCAD_ADD_TEST(NAME bn_plane_distsq_pt3_lseg3_v2_case2_1   COMMAND bn_test plane_pt 4 -5,-5,-5 5,5,5 5,5,5 2 0)
BRLCAD_ADD_TEST(NAME bn_plane_distsq_pt3_lseg3_v2_case3_1   COMMAND bn_test plane_pt 4 -5,-5,-5 5,5,5 -10,-5,-10 3 50)
BRLCAD_ADD_TEST(NAME bn_plane_distsq_pt3_lseg3_v2_case4_1   COMMAND bn_test plane_pt 4 -5,-5,-5 5,5,5 5,10,10 4 50)
BRLCAD_ADD_TEST(NAME bn_plane_distsq_pt3_lseg3_v2_case5_1   COMMAND bn_test plane_pt 4 -5,-5,-5 5,5,5 0,-5,-5 5 16.666666)

# For function #5 (bn_mk_plane_3pts), the <args> format is as follows:
#
#  A B C <expected return
#
# Where A, B, and C are the points in question. This test function
# does not currently support checking the resultant plane.

BRLCAD_ADD_TEST(NAME bn_plane_mk_plane_3pts_fail_same        COMMAND bn_test plane_pt 5 0,0,0 0,0,0 0,0,0 -1)
BRLCAD_ADD_TEST(NAME bn_plane_mk_plane_3pts_fail_collinear_1 COMMAND bn_test plane_pt 5 0,0,0 5,8,2 0,0,0 -1)
BRLCAD_ADD_TEST(NAME bn_plane_mk_plane_3pts_fail_collinear_2 COMMAND bn_test plane_pt 5 5,8,2 0,0,0 0,0,0 -1)
BRLCAD_ADD_TEST(NAME bn_plane_mk_plane_3pts_fail_collinear_3 COMMAND bn_test plane_pt 5 -10,-10,-10 0,0,0 5,5,5 -1)
BRLCAD_ADD_TEST(NAME bn_plane_mk_plane_3pts_succeed          COMMAND bn_test plane_pt 5 -10,-10,-10 7,3,8 19,4,2 0)


# For COMMAND bn_test plane_dist, the input format is as follows:
#
# COMMAND bn_test plane_dist <function number> <args>

# For function #1 (bn_dist_pt3_line3), the <args> format is as follows:
#
# Ax,Ay,Az DIRx,DIRy,DIRz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz
#
# where A and DIR define the line segment, and P is the point in
# question. An individual point is three integer or floating point
# numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_line3_case0_1   COMMAND bn_test plane_dist 2 -5,-5,-5 1,1,1 -5,-5,-5 0 0 -5,-5,-5)
BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_line3_case1_1   COMMAND bn_test plane_dist 2 -5,-5,-5 1,1,1 0,0,0 1 0.5 0,0,0)
BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_line3_case2_1   COMMAND bn_test plane_dist 2 -5,-5,-5 5,5,5 0,-5,-5 2 4.08248 -3.33333,-3.33333,-3.33333)

# For function #2 (bn_dist_pt3_lseg3), the <args> format is as follows:
#
# Ax,Ay,Az Bx,By,Bz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz
#
# where A and B define the line segment, and P is the point in
# question. An individual point is three integer or floating point
# numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_lseg3_case0_1   COMMAND bn_test plane_dist 2 -5,-5,-5 5,5,5 0,0,0 0 0.5 0,0,0)
BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_lseg3_case1_1   COMMAND bn_test plane_dist 2 -5,-5,-5 5,5,5 -5,-5,-5 1 0 -5,-5,-5)
BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_lseg3_case2_1   COMMAND bn_test plane_dist 2 -5,-5,-5 5,5,5 5,5,5 2 0 5,5,5)
BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_lseg3_case3_1   COMMAND bn_test plane_dist 2 -5,-5,-5 5,5,5 -10,-5,-10 3 7.07107 -5,-5,-5)
BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_lseg3_case4_1   COMMAND bn_test plane_dist 2 -5,-5,-5 5,5,5 5,10,10 4 7.07107 5,5,5)
BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_lseg3_case5_1   COMMAND bn_test plane_dist 2 -5,-5,-5 5,5,5 0,-5,-5 5 4.08248 -3.33333,-3.33333,-3.33333)

# For function #3 (bn_dist_pt3_pt3), the <args> format is as follows:
#
# Ax,Ay,Az Bx,By,Bz expected_return
#
# where A and B are the points in question. An individual point is
# three integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_pt3_same   COMMAND bn_test plane_dist 3 0,0,0 0,0,0 0)
BRLCAD_ADD_TEST(NAME bn_plane_dist_pt3_pt3_1      COMMAND bn_test plane_dist 3 0,0,0 1,1,1 1.732051)

# For COMMAND bn_test plane_isect, the input format is as follows:
#
# COMMAND bn_test plane_isect <function number> <args>

# For function #1 (bn_isect_line_lseg), the <args> format is as follows:
#
# Px,Py,Pz Dx,Dy,Dz Ax,Ay,Az Bx,By,Bz expected_return expected_t
#
# where P and D define the parametric line, and A and B define the
# line segment. An individual point is three integer or floating point
# numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_plane_isect_line_lseg_case-4_1   COMMAND bn_test plane_isect 1 -5,-5,-5 1,1,1 0,0,0 0,0,0 -4 0)
BRLCAD_ADD_TEST(NAME bn_plane_isect_line_lseg_case-3_1   COMMAND bn_test plane_isect 1 -5,-5,-5 1,1,1 0,0,1 0,0,5 -3 8.660)
BRLCAD_ADD_TEST(NAME bn_plane_isect_line_lseg_case-2_1   COMMAND bn_test plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,-1 -2 8.660)
BRLCAD_ADD_TEST(NAME bn_plane_isect_line_lseg_case-1_1   COMMAND bn_test plane_isect 1 -5,-5,-5 1,1,1 -5,-5,-6 0,0,-1 -1 0)
BRLCAD_ADD_TEST(NAME bn_plane_isect_line_lseg_case0_1    COMMAND bn_test plane_isect 1 -5,-5,-5 1,1,1 1,1,1 5,5,5 0 10.392)
BRLCAD_ADD_TEST(NAME bn_plane_isect_line_lseg_case1_1    COMMAND bn_test plane_isect 1 -5,-5,-5 1,1,1 0,0,0 0,0,5 1 8.660)
BRLCAD_ADD_TEST(NAME bn_plane_isect_line_lseg_case2_1    COMMAND bn_test plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,0 2 8.660)
BRLCAD_ADD_TEST(NAME bn_plane_isect_line_lseg_case3_1    COMMAND bn_test plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,5 3 8.660)

# For function #2 (bn_isect_lseg3_lseg3), the <args> format is as follows:
#
# Px,Py,Pz PDIRx,PDIRy,PDIRz Qx,Qy,Qz QDIRx,QDIRy,QDIRz expected_return expected_dist_0 expected_dist_1
#
# Where P and PDIR define the first segment, and Q and QDIR define the
# second. expected_dist_0 and expected_dist_1 are the expected values
# of the two components of the dist[] array output by the function. An
# individual point is three integer or floating point numbers
# separated by commas.

BRLCAD_ADD_TEST(NAME bn_plane_isect_lseg3_lseg3_case-3_1   COMMAND bn_test plane_isect 2 0,0,0 0,1,1 0,0,1 0,-1,1 -3 0.5 -0.5)
BRLCAD_ADD_TEST(NAME bn_plane_isect_lseg3_lseg3_case-3_2   COMMAND bn_test plane_isect 2 0,0,0 0,1,1 0,1,0 0,-0.25,0.25 -3 0.5 2)
BRLCAD_ADD_TEST(NAME bn_plane_isect_lseg3_lseg3_case-2_1   COMMAND bn_test plane_isect 2 0,0,0 0,1,1 1,1,1 0,1,1 -2 0 0)
BRLCAD_ADD_TEST(NAME bn_plane_isect_lseg3_lseg3_case-1_1   COMMAND bn_test plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -1,-1,-1 -1 1.666667 1.333333)
BRLCAD_ADD_TEST(NAME bn_plane_isect_lseg3_lseg3_case0_1    COMMAND bn_test plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -2,-2,-2 0 1.666667 1)
BRLCAD_ADD_TEST(NAME bn_plane_isect_lseg3_lseg3_case0_2    COMMAND bn_test plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -3,-3,-3 0 1.666667 0.666667)
BRLCAD_ADD_TEST(NAME bn_plane_isect_lseg3_lseg3_case0_3    COMMAND bn_test plane_isect 2 -5,-5,-5 5,5,5 -4,-4,-4 2,2,2 0 0.2 0.6)
BRLCAD_ADD_TEST(NAME bn_plane_isect_lseg3_lseg3_case1_1    COMMAND bn_test plane_isect 2 0,0,0 0,1,1 0,1,0 0,-1,1 1 0.5 0.5)

#
#  ************ complex.c tests *************

# For COMMAND bn_test complex, the input format is as follows:
#
# COMMAND bn_test complex <function number> <args>

# For function #1 (bn_cx_div), the <args> format is as follows:
#
# AP BP expected_return
#
# where AP and BP are the numbers in question. An individual complex
# number is two integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_cx_div_same_1        COMMAND bn_test complex 1  1,3 1,3 1,0)
BRLCAD_ADD_TEST(NAME bn_cx_div_same_2        COMMAND bn_test complex 1  3,1 3,1 1,0)
BRLCAD_ADD_TEST(NAME bn_cx_div_dif_1         COMMAND bn_test complex 1  6.3,4.2 9.8,7.7 0.605678,-0.0473186)
BRLCAD_ADD_TEST(NAME bn_cx_div_dif_2         COMMAND bn_test complex 1  0,0 1,1 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_div_by_0_1        COMMAND bn_test complex 1  1,1 0,0 1.0e20,1.0e20)

# For function #2 (bn_cx_mul/bn_cx_mul2), the <args> format is as follows:
#
# AP BP expected_return
#
# where AP and BP are the numbers in question. An individual complex
# number is two integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_cx_mul_0_1           COMMAND bn_test complex 2 5.2,3.8 0,0 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_mul_0_2           COMMAND bn_test complex 2 0,0 5.2,3.8 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_mul_inv_1         COMMAND bn_test complex 2 7.4,2.3 0.123231,-0.0383014 1,0)
BRLCAD_ADD_TEST(NAME bn_cx_mul_inv_2         COMMAND bn_test complex 2 0.123231,-0.0383014 7.4,2.3 1,0)

# For function #3 (bn_cx_sub), the <args> format is as follows:
#
# AP BP expected_return
#
# where AP and BP are the numbers in question. An individual complex
# number is two integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_cx_sub_0_1           COMMAND bn_test complex 3 5.2,3.8 0,0 5.2,3.8)
BRLCAD_ADD_TEST(NAME bn_cx_sub_0_2           COMMAND bn_test complex 3 0,0 5.2,3.8 -5.2,-3.8)
BRLCAD_ADD_TEST(NAME bn_cx_sub_same_1        COMMAND bn_test complex 3 1.4,2.9 1.4,2.9 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_sub_dif_1         COMMAND bn_test complex 3 7.4,-2.3 8.9,6.4 -1.5,-8.7)
BRLCAD_ADD_TEST(NAME bn_cx_sub_dif_2         COMMAND bn_test complex 3 8.9,6.4 7.4,-2.3 1.5,8.7)

# For function #4 (bn_cx_add), the <args> format is as follows:
#
# AP BP expected_return
#
# where AP and BP are the numbers in question. An individual complex
# number is two integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_cx_add_0_1           COMMAND bn_test complex 4 5.2,3.8 0,0 5.2,3.8)
BRLCAD_ADD_TEST(NAME bn_cx_add_0_2           COMMAND bn_test complex 4 0,0 5.2,3.8 5.2,3.8)
BRLCAD_ADD_TEST(NAME bn_cx_add_inv_1         COMMAND bn_test complex 4 1.4,2.9 -1.4,-2.9 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_add_dif_1         COMMAND bn_test complex 4 7.4,-2.3 8.9,6.4 16.3,4.1)
BRLCAD_ADD_TEST(NAME bn_cx_add_dif_2         COMMAND bn_test complex 4 8.9,6.4 7.4,-2.3 16.3,4.1)

# For function #5 (bn_cx_sqrt), the <args> format is as follows:
#
# IP expected_return
#
# where IP is the number in question. An individual complex number is
# two integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_cx_sqrt_1            COMMAND bn_test complex 5  0,2 1,1)
BRLCAD_ADD_TEST(NAME bn_cx_sqrt_2            COMMAND bn_test complex 5  2,0 1.414214,0)
BRLCAD_ADD_TEST(NAME bn_cx_sqrt_3            COMMAND bn_test complex 5  0,0 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_sqrt_4            COMMAND bn_test complex 5 6.3,4.2 2.63360,0.797389)
BRLCAD_ADD_TEST(NAME bn_cx_sqrt_5            COMMAND bn_test complex 5 9.8,7.7 3.33640,1.15394)

# For function #6 (bn_cx_neg), the <args> format is as follows:
#
# IP expected_return
#
# where IP is the number in question. An individual complex number is
# two integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_cx_neg_0_1           COMMAND bn_test complex 6  0,0 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_neg_0_2           COMMAND bn_test complex 6  2,0 -2,0)
BRLCAD_ADD_TEST(NAME bn_cx_neg_0_3           COMMAND bn_test complex 6  0,2 0,-2)
BRLCAD_ADD_TEST(NAME bn_cx_neg_dif_1         COMMAND bn_test complex 6 6.3,4.2 -6.3,-4.2)

# For function #7 (bn_cx_conj), the <args> format is as follows:
#
# IP expected_return
#
# where IP is the number in question. An individual complex number is
# two integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_cx_conj_0_1          COMMAND bn_test complex 7  0,0 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_conj_0_2          COMMAND bn_test complex 7  2,0 2,0)
BRLCAD_ADD_TEST(NAME bn_cx_conj_0_3          COMMAND bn_test complex 7  0,2 0,-2)
BRLCAD_ADD_TEST(NAME bn_cx_conj_dif_1        COMMAND bn_test complex 7 6.3,4.2 6.3,-4.2)

# For function #8 (bn_cx_real/bn_cx_imag), the <args> format is as follows:
#
# IP expected_re,expected_im
#
# where IP is the number in question. An individual complex number is
# two integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_cx_parts_1           COMMAND bn_test complex 8  0,0 0,0)
BRLCAD_ADD_TEST(NAME bn_cx_parts_2           COMMAND bn_test complex 8  1,2 1,2)
BRLCAD_ADD_TEST(NAME bn_cx_parts_3           COMMAND bn_test complex 8  2,1 2,1)


#
#  *************** mat.c tests ***************
#
# For COMMAND bn_test mat, input is as follows:
# COMMAND bn_test mat <function_number> <args>
#
# Values supplied to the test program were verified using
# Wolfram Alpha: http://www.wolframalpha.com/
# and http://www.bluebit.gr/matrix-calculator/

# For function #1 (bn_mat_mul), the <args> format is as follows:
#
# M1 M2 <expected_result>
#
# where M1 is the first matrix, M2 is the second matrix.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_mul_0_1          COMMAND bn_test mat 1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1.0,2.1,3.3,4.4,5.5,6.6,7.7,8.8,9.9,10.1,11.11,12.12,13.13,14.14,15.15,16.16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_mul_0_2          COMMAND bn_test mat 1 2.8,1.7,3205.555,87.76,55.0,66.0,77.0,0.0,0.0,10.1,46.8,537.999,13.0,14.0,15.0,24382.5373 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_mul_id_1         COMMAND bn_test mat 1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 33.34,28.7135672,44.84,55.85,1,1,0,0,0,7843.4444,11,12,473.232,83.17,75.0,8.417 33.34,28.7135672,44.84,55.85,1,1,0,0,0,7843.4444,11,12,473.232,83.17,75.0,8.417)
BRLCAD_ADD_TEST(NAME bn_mat_mul_id_2         COMMAND bn_test mat 1 3688.701794246889,5712.945961512324,8172.282142910184,5109.8260742578495,2697.196099611537,2586.742536023153,5067.2882336104285,1149.9923643570387,6827.968448547921,9548.849169087358,5418.682604729545,9117.6798488191,5478.474626204755,7525.19019682893,3172.945417883698,8609.907811093646 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 3688.701794246889,5712.945961512324,8172.282142910184,5109.8260742578495,2697.196099611537,2586.742536023153,5067.2882336104285,1149.9923643570387,6827.968448547921,9548.849169087358,5418.682604729545,9117.6798488191,5478.474626204755,7525.19019682893,3172.945417883698,8609.907811093646)
BRLCAD_ADD_TEST(NAME bn_mat_mul_id_id        COMMAND bn_test mat 1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_mul_dif_1        COMMAND bn_test mat 1 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1 80,70,60,50,240,214,188,162,400,358,316,274,560,502,444,386)
BRLCAD_ADD_TEST(NAME bn_mat_mul_dif_2        COMMAND bn_test mat 1 3.2,62.9,53.0,71.5,98.6,75.4,9.6,89.9,37.7,49.0,60.2,55.2,27.8,40.2,0.5,78.1 37.1,73.1,4.9,72.1,68.2,64.7,75.7,87.4,85.8,45.0,94.5,30.8,95.9,45.9,45.4,49.1 15812.75,9970.4,13031.81,10871.23,18245.43,16644.45,11179.58,18408.79,15199.31,11168.85,12089.01,11565.25,11305.71,8240.41,6772.35,9367.97)

# For function #2 (bn_mat_mul3), the <args> format is as follows:
#
# M1 M2 M3 <expected_result>
#
# where M1 M2 M3 are the three matrices to be multiplied.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_mul3_0_1         COMMAND bn_test mat 2 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_mul3_0_id        COMMAND bn_test mat 2 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_mul3_id_1        COMMAND bn_test mat 2 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1 80,70,60,50,240,214,188,162,400,358,316,274,560,502,444,386)
BRLCAD_ADD_TEST(NAME bn_mat_mul3_dif         COMMAND bn_test mat 2 3.1,4.2,1.9,1.8,2.9,0.3,4.1,4.5,2.9,2.7,0.7,1.7,1.6,0.8,4.5,5.0 1.1,2.1,2.3,3.5,2.7,1.8,2.6,3.8,1.5,0.7,3.0,4.4,2.3,0.3,4.5,4.3 3.2,2.0,2.6,0.0,2.1,4.0,0.5,1.6,4.3,3.1,2.7,4.7,0.9,3.3,2.4,4.7 278.616,347.578,253.473,376.876,304.197,369.044,283.557,434.156,203.015,252.984,183.327,274.048,315.303,376.398,294.975,446.11)

# For function #3 (bn_mat_mul4), the <args> format is as follows:
#
# M1 M2 M3 M4 <expected_result>
#
# where M1 M2 M3 M4 are the four matrices to be multiplied.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_mul4_id_4        COMMAND bn_test mat 3 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_mul4_0_4         COMMAND bn_test mat 3 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_mul4_id_0        COMMAND bn_test mat 3 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_mul4_same        COMMAND bn_test mat 3 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 113960,129040,144120,159200,263272,298128,332984,367840,412584,467216,521848,576480,561896,636304,710712,785120)

# For function #4 (bn_matXvec), the <args> format is as follows:
#
# M V <expected_result>
#
# where M is the matrix
#       V is the (column) vector to which the matrix will be multiplied, MxV.
# A matrix is 16 floating point numbers separated by commas.
# A vector is 4 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_matXvec_id           COMMAND bn_test mat 4 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 2.78,3.45,16.7,38.22 2.78,3.45,16.7,38.22)
BRLCAD_ADD_TEST(NAME bn_matXvec_0_1          COMMAND bn_test mat 4 18.59,25.82,46.39,16.56,46.13,22.16,29.66,92.89,67.58,79.96,59.47,51.4,55.83,48.5,82.51,73.52 0,0,0,0 0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_matXvec_0_2          COMMAND bn_test mat 4 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1.1,2.2,3.3,4.4 0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_matXvec_dif_1        COMMAND bn_test mat 4 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4 30,70,110,150)
BRLCAD_ADD_TEST(NAME bn_matXvec_dif_2        COMMAND bn_test mat 4 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 0,1,2,3 14,38,62,86)
BRLCAD_ADD_TEST(NAME bn_matXvec_dif_3        COMMAND bn_test mat 4 18.59,25.82,46.39,16.56,46.13,22.16,29.66,92.89,67.58,79.96,59.47,51.4,55.83,48.5,82.51,73.52 7.31,68.38,65.97,3.26  5015.7984,4112.0027,10052.4745,9407.4072)

# For function #5 (bn_mat_inverse), the <args> format is as follows:
#
# 0|1 M [expected_result]
#
# where 0 or 1 indicates whether the matrix is singular
#       M is the matrix to be inverted.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_inverse_0        COMMAND bn_test mat 5 1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_inverse_id       COMMAND bn_test mat 5 0 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_inverse_sing_1   COMMAND bn_test mat 5 1 1,2,2,0,1,1,3,4,2,2,1,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_inverse_sing_2   COMMAND bn_test mat 5 1 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)
BRLCAD_ADD_TEST(NAME bn_mat_inverse_1        COMMAND bn_test mat 5 0 14.16,71.27,95.53,77.27,91.51,26.83,17.56,6.94,30.11,40.47,38.1,39.27,56.52,90.12,87.96,84.3 0.00836179,0.0114599,0.0844829,-0.0479631,-0.0600379,-0.0107874,-0.349984,0.218954,0.0425546,0.0207413,-0.0527631,-0.0161344,0.0141745,-0.0177931,0.372557,-0.173216)

# For function #6 (bn_mat_trn), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to transpose.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_trn_id           COMMAND bn_test mat 6 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_trn_same         COMMAND bn_test mat 6 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
BRLCAD_ADD_TEST(NAME bn_mat_trn_1            COMMAND bn_test mat 6 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,5,9,13,2,6,10,14,3,7,11,15,4,8,12,16)
BRLCAD_ADD_TEST(NAME bn_mat_trn_2            COMMAND bn_test mat 6 99.01,41.88,17.07,37.47,38.8,42.89,34.48,82.84,59.97,23.74,6.98,27.81,1.64,86.35,43.57,13.87 99.01,38.8,59.97,1.64,41.88,42.89,23.74,86.35,17.07,34.48,6.98,43.57,37.47,82.84,27.81,13.87)

# For function #7 (bn_mat_is_identity), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to test.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_is_identity_id   COMMAND bn_test mat 7 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
BRLCAD_ADD_TEST(NAME bn_mat_is_identity_0    COMMAND bn_test mat 7 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
BRLCAD_ADD_TEST(NAME bn_mat_is_identity_1    COMMAND bn_test mat 7 15.58,76.15,18.45,80.55,52.36,28.49,85.75,56.75,76.44,66.57,32.39,52.18,39.21,96.09,98.66,30.95 0)
BRLCAD_ADD_TEST(NAME bn_mat_is_identity_2    COMMAND bn_test mat 7 1.001,0,0,0,0,1.001,0,0,0,0,1.001,0,0,0,0,1.001 0)

# For function #8 (bn_mat_det3), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to find the upper-left 3x3 determinant of.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_det3_id          COMMAND bn_test mat 8 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
BRLCAD_ADD_TEST(NAME bn_mat_det3_0           COMMAND bn_test mat 8 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
BRLCAD_ADD_TEST(NAME bn_mat_det3_1           COMMAND bn_test mat 8 77.64,50.22,46.68,16.35,41.25,71.79,24.01,23.88,37.37,94.83,34.25,4.43,95.01,81.8,1.45,45.57 45601.558488)
BRLCAD_ADD_TEST(NAME bn_mat_det3_2           COMMAND bn_test mat 8 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0)

# For function #9 (bn_mat_determinant), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to find the determinant of.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_determinant_id   COMMAND bn_test mat 9 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
BRLCAD_ADD_TEST(NAME bn_mat_determinant_0    COMMAND bn_test mat 9 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
BRLCAD_ADD_TEST(NAME bn_mat_determinant_1    COMMAND bn_test mat 9 80.22,28.45,95.46,74.05,53.38,74.98,71.55,24.86,65.01,46.42,64.24,49.13,75.74,79.22,64.61,17.33 2.1004520202541295e6)
BRLCAD_ADD_TEST(NAME bn_mat_determinant_2    COMMAND bn_test mat 9 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0)

# For function #10 (bn_mat_is_equal), the <args> format is as follows:
#
# M1 M2 <expected_result>
#
# where M1 and M2 are the two matrices to be compared.
# A matrix is 16 floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_is_equal_1       COMMAND bn_test mat 10 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1)
BRLCAD_ADD_TEST(NAME bn_mat_is_equal_2       COMMAND bn_test mat 10 70.46,38.7,1.57,84.19,38.53,52.52,89.28,8.58,89.96,61.36,5.45,63.62,99.98,82.15,65.44,67.46 70.46,38.7,1.57,84.19,38.53,52.52,89.28,8.58,89.96,61.36,5.45,63.62,99.98,82.15,65.44,67.46 1)
BRLCAD_ADD_TEST(NAME bn_mat_is_equal_3       COMMAND bn_test mat 10 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
BRLCAD_ADD_TEST(NAME bn_mat_is_equal_4       COMMAND bn_test mat 10 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 71.53,18.85,47.48,24.56,42.34,98.18,42.1,74.61,30.47,49.18,63.06,2.11,58.31,60.61,0.62,34.23 0)

# For function #11 (bn_mat_atan2), the <args> format is as follows:
#
# y x <expected_result>
#
# where x and y are the floating point arguments to the atan2 function

BRLCAD_ADD_TEST(NAME bn_atan2_0_0            COMMAND bn_test mat 11 0 0 0)
BRLCAD_ADD_TEST(NAME bn_atan2_0_1            COMMAND bn_test mat 11 2.8 0 1.570796326794896)
BRLCAD_ADD_TEST(NAME bn_atan2_0_2            COMMAND bn_test mat 11 -2.8 0 -1.570796326794896)
BRLCAD_ADD_TEST(NAME bn_atan2_1              COMMAND bn_test mat 11 2.55 8.76 0.283268001141651)
BRLCAD_ADD_TEST(NAME bn_atan2_2              COMMAND bn_test mat 11 -2.8 -7.2 -2.770701364777130)

# For function #12 (bn_vtoh_move), the <args> format is as follows:
#
# V <expected_result>
#
# where V is the 3-vector to move to a homogeneous vector.
# A vector is three floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_vtoh_move            COMMAND bn_test mat 12 2.6,-5.5,8.12 2.6,-5.5,8.12,1)

# For function #13 (bn_vtoh_move), the <args> format is as follows:
#
# H <expected_result>
#
# where H is the homogeneous vector to move to a 3-vector.
# A homogeneous vector is four floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_htov_move            COMMAND bn_test mat 13 1.1,2.2,3.3,7.4 0.14864864864864866,0.2972972972972973,0.4459459459459459)

# For function #14 (bn_mat_ae), the <args> format is as follows:
#
# az el <expected_result>
#
# where az is the azimuth and el is the elevation to generate a
# transform matrix.
# az and el are floating point numbers and are in degrees.

BRLCAD_ADD_TEST(NAME bn_mat_ae               COMMAND bn_test mat 14 0 90 0,0,-1,0,0,1,0,0,1,0,0,0,0,0,0,1)

# For function #15 (bn_ae_vec), the <args> format is as follows:
#
# V <expected_az> <expected_el>
#
# where V is the vector to extract az and el info from.
#
# Functions 15-17 were verified using the website below.
# http://www.random-science-tools.com/maths/coordinate-converter.htm

BRLCAD_ADD_TEST(NAME bn_ae_vec_0             COMMAND bn_test mat 15 0,1,0 90 0)
BRLCAD_ADD_TEST(NAME bn_ae_vec_1             COMMAND bn_test mat 15 5.7,-2.34,19.61 337.68055960702 72.556852698595)
BRLCAD_ADD_TEST(NAME bn_ae_vec_2             COMMAND bn_test mat 15 -23.78,42,3.141 119.518125952119 3.7234738236524)

# For function #16 (bn_vec_ae), the <args> format is as follows:
#
# az el <expected_result>
#
# where az is the azimuth and el is the elevation to generate a vector
# az and el are floating point numbers and are in radians.

# These two tests fail for some reason. It could be a bug in bn_vec_ae().
# The value of the returned vector seems to always have the same z value
# irrespective of the input.

BRLCAD_ADD_TEST(NAME bn_vec_ae_1             COMMAND bn_test mat 16 1 1 0.291926581820,0.454648713558,0.841470984697)
BRLCAD_ADD_TEST(NAME bn_vec_ae_2             COMMAND bn_test mat 16 0.5 1.2 0.317998846662,0.173723561699,0.932039085893)

# For function #17 (bn_vec_aed), the <args> format is as follows:
#
# az el d <expected_result>
#
# where az is the azimuth, el is the elevation and d is the distance
# to generate a vector
# az and el are floating point numbers and are in radians.
# d is a floating point number.

BRLCAD_ADD_TEST(NAME bn_vec_aed_1            COMMAND bn_test mat 17 1 1 5 1.45963290910,2.27324356779,4.20735492349)
BRLCAD_ADD_TEST(NAME bn_vec_aed_2            COMMAND bn_test mat 17 0.3845 0.286 18.3354 16.3062567701,6.59816438267,5.17272752864)

# For function #18 (bn_mat_angles), the <args> format is as follows:
#
# x y z <expected_result>
#
# where x, y, z are the angles to rotate about their respective axes
# x, y, z are floating point numbers and are in degrees.
#
# Functions 18 and 19 were verified using the website below
# http://toolserver.org/~dschwen/tools/rotationmatrix.html

BRLCAD_ADD_TEST(NAME bn_mat_angles_1         COMMAND bn_test mat 18 30 40 50 0.4924038765,-0.5868240888,0.6427876097,0,0.8700019038,0.310468461,-0.3830222216,0,0.0252013863,0.7478280708,0.6634139482,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_angles_2         COMMAND bn_test mat 18 84.23 19.5 38.9 0.733604282,-0.591944033,0.3338068592,0,0.3215992025,-0.1303153824,-0.9378655842,0,0.5986641049,0.7953742282,0.0947688064,0,0,0,0,1)

# For function #19 (bn_mat_angles_rad), the <args> format is as follows:
#
# x y z <expected_result>
#
# where x, y, z are the angles to rotate about their respective axes
# x, y, z are floating point numbers and are in radians.

BRLCAD_ADD_TEST(NAME bn_mat_angles_rad_1     COMMAND bn_test mat 19 1.14 0.856 0.321 0.6219827767,-0.2068090198,0.7552267572,0,0.7829298453,0.1797494268,-0.5955761923,0,-0.0125810482,0.9617277021,0.2737180014,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_angles_rad_2     COMMAND bn_test mat 19 0.6 1 0.92 0.3273260276,-0.4298653899,0.8414709848,0,0.9444818397,0.1219905484,-0.3050776304,0,0.0284908076,0.8946139126,0.4459307359,0,0,0,0,1)

# For function #20 (bn_eigen2x2), the <args> format is as follows:
#
# a b c <expected_val1> <expected_val2> <expected_vec1> <expected_vec2>
#
# where a, b, c are the values of the symmetric 2x2 matrix.
# The eigenvalue with the lowest absolute value will be val1, and its
# eigenvector will be in vec1.
# An eigen vector is two floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bn_eigen2x2_1           COMMAND bn_test mat 20 2.18 13.9 6.6 -9.68459058019096 18.464590580191 0.760598630446286,-0.649222398999938,0 0.649222398999938,0.760598630446286,0)
BRLCAD_ADD_TEST(NAME bn_eigen2x2_2           COMMAND bn_test mat 20 1 2 3 -0.23606797749979 4.23606797749979 0.85065080835204,-0.525731112119133,0 0.525731112119133,0.85065080835204,0)

# For function #21 (bn_mat_xrot), the <args> format is as follows:
#
# sinx cosx <expected_result>
#
# where sinx and cosx are the sine and cosine of the angle of rotation
# about the x-axis.
# sinx and cosx are floating point values.

BRLCAD_ADD_TEST(NAME bn_mat_xrot_1           COMMAND bn_test mat 21 0.342020143325668 0.939692620785908 1,0,0,0,0,0.9396926208,-0.3420201433,0,0,0.3420201433,0.9396926208,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_xrot_2           COMMAND bn_test mat 21 -0.78260815685241 0.622514636637619 1,0,0,0,0,0.6225146366,0.7826081569,0,0,-0.7826081569,0.6225146366,0,0,0,0,1)

# For function #22 (bn_mat_yrot), the <args> format is as follows:
#
# siny cosy <expected_result>
#
# where siny and cosy are the sine and cosine of the angle of rotation
# about the y-axis.
# siny and cosy are floating point values.

BRLCAD_ADD_TEST(NAME bn_mat_yrot_1           COMMAND bn_test mat 22 0.30901699437494 0.951056516295153 0.9510565163,0,-0.3090169944,0,0,1,0,0,0.3090169944,0,0.9510565163,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_yrot_2           COMMAND bn_test mat 22 0.17364817766693 0.984807753012208 0.9848077530122,0,-0.1736481777,0,0,1,0,0,0.1736481777,0,0.984807753,0,0,0,0,1)

# For function #23 (bn_mat_zrot), the <args> format is as follows:
#
# sinz cosz <expected_result>
#
# where sinz and cosz are the sine and cosine of the angle of rotation
# about the z-axis.
# sinz and cosz are floating point values.

BRLCAD_ADD_TEST(NAME bn_mat_zrot_1           COMMAND bn_test mat 23 0.669130606358858 0.743144825477394 0.743144825477394,-0.669130606358858,0,0,0.669130606358858,0.743144825477394,0,0,0,0,1,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_mat_zrot_2           COMMAND bn_test mat 23 -0.99619469809174 -0.087155742747658 -0.087155742747658,0.996194698091745,0,0,-0.996194698091746,-0.087155742747658,0,0,0,0,1,0,0,0,0,1)

# For function #24 (bn_mat_scale_about_pt), the <args> format is as follows:
#
# P s <expected_result>
#
# where P is the point to scale about and s is the uniform scale to
# be applied.
# A point is three floating point numbers separated by commas.
# Scale is a floating point number.

BRLCAD_ADD_TEST(NAME bn_mat_scale_about_pt_1 COMMAND bn_test mat 24 3,4,5 1.5 0 1,0,0,-1,0,1,0,-1.333333333333333,0,0,1,-1.666666666666667,0,0,0,0.666666666666667)
BRLCAD_ADD_TEST(NAME bn_mat_scale_about_pt_2 COMMAND bn_test mat 24 2.18,-4.55,-17.4 0.31 0 1,0,0,4.852258064516120,0,1,0,-10.127419354838700,0,0,1,-38.729032258064500,0,0,0,3.225806451612900)

# For function #25 (bn_mat_xform_about_pt), the <args> format is as follows:
#
# M P <expected_result>
#
# where M is the transform matrix, P is the point to apply from.
# A matrix is 16 floating point values separated by commas.
# A point is three floating point values separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_xform_about_pt_1 COMMAND bn_test mat 25 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 -2,4.5,6.7 -25,-26,-27,219.9,63.5,69,74.5,-602.65,96.1,103.8,111.5,-902.75,13,14,15,-121.5)

# For function #26 (bn_mat_ck), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to check for axis perpendicularity
# A matrix is 16 floating point values separated by commas.

BRLCAD_ADD_TEST(NAME bn_mat_ck_1             COMMAND bn_test mat 26 -0.064769339233561,-0.740316935054896,-0.669130606358858,0,-0.920939044198179,-0.213876288688268,0.325773249374894,0,-0.384286624235859,0.637328619165857,-0.667934144677115,0,0,0,0,1 0)
BRLCAD_ADD_TEST(NAME bn_mat_ck_2             COMMAND bn_test mat 26 -0.384795044679446,-0.906520316365329,0.17364817766693,23.835,-0.772197474581702,0.419235049422787,0.477444272753497,-1.23,-0.505612335529687,0.049627506006009,-0.861332268528144,-46.321,0,0,0,1 0)
BRLCAD_ADD_TEST(NAME bn_mat_ck_3             COMMAND bn_test mat 26 -0.38457345,-0.944,0.46483,23.835,0.27474,0.4192787,0.4272753497,-1.23,-0.87,0.06009,0.999144,-46.321,0,0,0,1 -1)

# For function #27 (bn_mat_dup), there is one argument to keep main()
# happy, which is ignored.
# A matrix is initiated, copied and the dereferenced pointer compared in the function.

BRLCAD_ADD_TEST(NAME bn_mat_dup              COMMAND bn_test mat 27 0)


# For function #28 (bn_mat_opt), two matrices are supplied.  The
# first is the expected answer - 16 floating point values separated
# by commas.  Subsequent arguments are the option input(s)

BRLCAD_ADD_TEST(NAME bn_mat_opt_idn_1          COMMAND bn_test mat 28 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 IDN)
BRLCAD_ADD_TEST(NAME bn_mat_opt_idn_2          COMMAND bn_test mat 28 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1})
BRLCAD_ADD_TEST(NAME bn_mat_opt_idn_3          COMMAND bn_test mat 28 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1)
BRLCAD_ADD_TEST(NAME bn_mat_opt_idn_4          COMMAND bn_test mat 28 27.7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,3.3 {27.7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,3.3})
BRLCAD_ADD_TEST(NAME bn_mat_opt_idn_5          COMMAND bn_test mat 28 27.7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,3.3 27.7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 3.3)

#  ***************** sobolseq.c tests ***************

BRLCAD_ADD_TEST(NAME bn_sobol_3_1000         COMMAND bn_test sobolseq 3 1000)

#
#  *************** tabdata.c tests ***************
#
# For COMMAND bn_test tabdata, input is as follows:
# COMMAND bn_test tabdata <function_number> <args>

# For function #1 (bn_table_make_uniform), the <args> format is as follows:
#
# first last num expected
#
# where first last and num will be used to make a uniform table, and
# expected is the expected result table.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx).

BRLCAD_ADD_TEST(NAME bn_table_make_uniform COMMAND bn_test tabdata 1 0 100 10 10,0,10,20,30,40,50,60,70,80,90,100)

# For function #2 (bn_tabdata_add), the <args> format is as follows:
#
# table tabdata1 tabdata2 expected_tabdata
#
# where tabdata1 and tabdata2 will be added, and table is a table that
# tabdata1 and tabdata2 use.  A table is a list of comma-separated
# numbers, where the first one is the number of entries:
# nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated numbers
# with the same length as the table to which it refers; if the table
# entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_add COMMAND bn_test tabdata 2 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 9,5,9,6,14)

# For function #3 (bn_tabdata_mul), the <args> format is as follows:
#
# table tabdata1 tabdata2 expected_tabdata
#
# where tabdata1 and tabdata2 will be multiplied, and table is a table
# that tabdata1 and tabdata2 use.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated
# numbers with the same length as the table to which it refers; if the
# table entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_mul COMMAND bn_test tabdata 3 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45)

# For function #4 (bn_tabdata_mul3), the <args> format is as follows:
#
# table tabdata1 tabdata2 tabdata3 expected_tabdata
#
# where tabdata1, tabdata2, and tabdata3 will be multiplied, and table
# is a table that tabdata1, tabdata2, and tabdata3 use.  A table is a
# list of comma-separated numbers, where the first one is the number
# of entries: nx,x0,x1,...,x(nx); a tabdata is a list of
# comma-separated numbers with the same length as the table to which
# it refers; if the table entry is nx,x0,x1,...,x(nx), then the
# tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_mul3 COMMAND bn_test tabdata 4 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45 64,36,324,64,2025)


# For function #5 (bn_tabdata_incr_mul3_scale), the <args> format is as follows:
#
# table tabdataout_orig tabdata1 tabdata2 tabdata3 scale expected_tabdata
#
# where tabdata1, tabdata2, and tabdata3, and scale will be
# multiplied, and the result will be added to tabdataout_orig; table
# is a table that tabdata1, tabdata2, and tabdata3 use.  A table is a
# list of comma-separated numbers, where the first one is the number
# of entries: nx,x0,x1,...,x(nx); a tabdata is a list of
# comma-separated numbers with the same length as the table to which
# it refers; if the table entry is nx,x0,x1,...,x(nx), then the
# tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_incr_mul3_scale COMMAND bn_test tabdata 5 5,0,20,40,60,80,100 5,4,3,2,1 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45 2 133,76,651,130,4051)


# For function #6 (bn_tabdata_incr_mul2_scale), the <args> format is as follows:
#
# table tabdataout_orig tabdata1 tabdata2 scale expected_tabdata
#
# where tabdata1 and tabdata2, and scale will be multiplied, and the
# result will be added to tabdataout_orig; table is a table that
# tabdata and tabdata2 use.  A table is a list of comma-separated
# numbers, where the first one is the number of entries:
# nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated numbers
# with the same length as the table to which it refers; if the table
# entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_incr_mul2_scale COMMAND bn_test tabdata 6 5,0,20,40,60,80,100 5,4,3,2,1 1,2,3,4,5 8,3,6,2,9 2 21,16,39,18,91)


# For function #7 (bn_tabdata_scale), the <args> format is as follows:
#
# table tabdata scale expected_tabdata
#
# where tabdata will be scaled by scale.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated
# numbers with the same length as the table to which it refers; if the
# table entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_scale COMMAND bn_test tabdata 7 5,0,20,40,60,80,100 1,2,3,4,5 2 2,4,6,8,10)


# For function #8 (bn_table_scale), the <args> format is as follows:
#
# table scale expected_table
#
# where table will be scaled by scale.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx).

BRLCAD_ADD_TEST(NAME bn_table_scale COMMAND bn_test tabdata 8 5,0,20,40,60,80,100 2 5,0,40,80,120,160,200)


# For function #9 (bn_tabdata_join1), the <args> format is as follows:
#
# table tabdata1 tabdata2 scale expected_tabdata
#
# where tabdata1, tabdata2, and scale will be combined via
# bn_tabdata_join1, and the result will be checked against
# expected_tabdata. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_join1 COMMAND bn_test tabdata 9 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2 11,10,9,8,7)


# For function #10 (bn_tabdata_join2), the <args> format is as follows:
#
# table tabdata1 tabdata2 tabdata3 scale1 scale2 expected_tabdata
#
# where tabdata1, tabdata2, and scale will be combined via
# bn_tabdata_join2, and the result will be checked against
# expected_tabdata. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_join2 COMMAND bn_test tabdata 10 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2,4,6,8,10 2 3 17,22,27,32,37)


# For function #11 (bn_tabdata_blend2), the <args> format is as follows:
#
# table tabdata1 tabdata2 scale1 scale2 expected_result
#
# where tabdata1, tabdata2, scale1, and scale2 will be combined via
# bn_tabdata_blend2, and the result will be checked against
# expected_result. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_blend2 COMMAND bn_test tabdata 11 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2 0.5 4.5,6,7.5,9,10.5)


# For function #12 (bn_tabdata_blend3), the <args> format is as follows:
#
# table tabdata1 tabdata2 tabdata3 scale1 scale2 scale3 expected_result
#
# where tabdata1, tabdata2, tabdata3, scale1, scale2, and scale3 will
# be combined via bn_tabdata_blend3, and the result will be checked
# against expected_result. A table is a list of comma-separated
# numbers, where the first one is the number of entries:
# nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated numbers
# with the same length as the table to which it refers; if the table
# entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_blend3 COMMAND bn_test tabdata 12 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 3,2,4,1,5 2 0.5 3 13.5,12,19.5,12,25.5)


# For function #13 (bn_tabdata_area1), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of the tabdata (which is a tabdata over table), will
# be calculated. The area will be calculated using interpretation #1
# (see bn.h). A table is a list of comma-separated numbers, where the
# first one is the number of entries: nx,x0,x1,...,x(nx); a tabdata is
# a list of comma-separated numbers with the same length as the table
# to which it refers; if the table entry is nx,x0,x1,...,x(nx), then
# the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_area1 COMMAND bn_test tabdata 13 5,0,20,40,60,80,100 1,2,3,4,5 15)


# For function #14 (bn_tabdata_area2), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of the tabdata (which is a tabdata over table), will
# be calculated. The area will be calculated using interpretation #2
# (see bn.h). A table is a list of comma-separated numbers, where the
# first one is the number of entries: nx,x0,x1,...,x(nx); a tabdata is
# a list of comma-separated numbers with the same length as the table
# to which it refers; if the table entry is nx,x0,x1,...,x(nx), then
# the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_area2 COMMAND bn_test tabdata 14 5,0,20,40,60,80,100 1,2,3,4,5 300)



# For function #15 (bn_tabdata_mul_area1), the <args> format is as follows:
#
# table tabdata1 tabdata2 expected_result
#
# where the area of tabdata1*tabdata2 (which are tabdatas over table),
# will be calculated. The area will be calculated using interpretation
# #1 (see bn.h). A table is a list of comma-separated numbers, where
# the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_mul_area1 COMMAND bn_test tabdata 15 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 35)


# For function #16 (bn_tabdata_mul_area2), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of tabdata1*tabdata2 (which are tabdatas over table),
# will be calculated. The area will be calculated using interpretation
# #2 (see bn.h). A table is a list of comma-separated numbers, where
# the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_mul_area2 COMMAND bn_test tabdata 16 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 700)


# For function #17 (bn_table_lin_interp), the <args> format is as follows:
#
# table tabdata wl expected_result
#
# where wl will be used to sample the tabdata via
# bn_table_lin_interp. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_table_lin_interp_1 COMMAND bn_test tabdata 17 5,0,20,40,60,80,100 1,2,3,4,5 30 2.5)
BRLCAD_ADD_TEST(NAME bn_table_lin_interp_2 COMMAND bn_test tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 30 3)
BRLCAD_ADD_TEST(NAME bn_table_lin_interp_3 COMMAND bn_test tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 45 3.5)
BRLCAD_ADD_TEST(NAME bn_table_lin_interp_4 COMMAND bn_test tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 25 2.5)
BRLCAD_ADD_TEST(NAME bn_table_lin_interp_5 COMMAND bn_test tabdata 17 5,0,20,40,60,80,100 1,2,3,4,5 800 0)


# For function #18 (bn_tabdata_copy), the <args> format is as follows:
#
# table tabdata
#
# where tabdata will be copied via bn_tabdata_copy. A table is a list
# of comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated
# numbers with the same length as the table to which it refers; if the
# table entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_copy COMMAND bn_test tabdata 18 5,0,20,40,60,80,100 1,2,3,4,5)


# For function #19 (bn_tabdata_dup), the <args> format is as follows:
#
# table tabdata
#
# where tabdata will be copied via bn_tabdata_dup. A table is a list
# of comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx); a tabdata is a list of comma-separated
# numbers with the same length as the table to which it refers; if the
# table entry is nx,x0,x1,...,x(nx), then the tabdata entry is
# y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_dup COMMAND bn_test tabdata 19 5,0,20,40,60,80,100 1,2,3,4,5)


# For function #20 (bn_tabdata_get_constval), the <args> format is as follows:
#
# table val expected_tabdata
#
# where a tabdata based on table with every item set to val will be
# produced via bn_tabdata_get_constval and checked against
# expected_tabdata. A table is a list of comma-separated numbers,
# where the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

BRLCAD_ADD_TEST(NAME bn_tabdata_get_constval COMMAND bn_test tabdata 20 5,0,20,40,60,80,100 3 3,3,3,3,3)

#
#  *************** qmath.c tests ***************
#
# For bn_qmath, input is as follows:
# bn_qmath <function_number> <args>

# For function #1 (quat_mat2quat), the <args> format is as follows:
#
# mat expected_quat
#
# where mat is a sequence of 16 comma-separated numbers representing a
# mat_t matrix, and expected_quat is a sequence of 4 comma-separated
# numbers representing a quaternion (X, Y, Z, W).

BRLCAD_ADD_TEST(NAME bn_quat_mat2quat_identity COMMAND bn_test qmath 1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_quat_mat2quat_trace_0_1 COMMAND bn_test qmath 1 1,0,0,0,0,-1,0,0,0,0,-1,0,0,0,0,-1 1,0,0,0)
BRLCAD_ADD_TEST(NAME bn_quat_mat2quat_trace_0_2 COMMAND bn_test qmath 1 -1,0,0,0,0,1,0,0,0,0,-1,0,0,0,0,-1 0,1,0,0)
BRLCAD_ADD_TEST(NAME bn_quat_mat2quat_trace_0_3 COMMAND bn_test qmath 1 -1,0,0,0,0,-1,0,0,0,0,1,0,0,0,0,-1 0,0,1,0)

# For function #2 (quat_quat2mat), the <args> format is as follows:
#
# quat expected_mat
#
# where quat is a sequence of 4 comma-separated numbers representing a
# quaternion (X, Y, Z, W), and expected_mat is a sequence of 16
# comma-separated numbers representing a mat_t matrix.

BRLCAD_ADD_TEST(NAME bn_quat_quat2mat_identity COMMAND bn_test qmath 2 0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_quat_quat2mat_trace_0_1 COMMAND bn_test qmath 2 1,0,0,0 1,0,0,0,0,-1,0,0,0,0,-1,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_quat_quat2mat_trace_0_2 COMMAND bn_test qmath 2 0,1,0,0 -1,0,0,0,0,1,0,0,0,0,-1,0,0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_quat_quat2mat_trace_0_3 COMMAND bn_test qmath 2 0,0,1,0 -1,0,0,0,0,-1,0,0,0,0,1,0,0,0,0,1)

# For function #3 (quat_distance), the <args> format is as follows:
#
# q1 q2 expected_distance
#
# where q1 and q2 are sequences of 4 comma-separated numbers each
# representing a quaternion (X, Y, Z, W), and expected_distance is the
# correct distance between q1 and q2.

BRLCAD_ADD_TEST(NAME bn_quat_distance_0 COMMAND bn_test qmath 3 1,2,0,3 1,2,0,3 0)
BRLCAD_ADD_TEST(NAME bn_quat_distance_1 COMMAND bn_test qmath 3 1,2,0,3 -1,-2,0,-3 7.483)
BRLCAD_ADD_TEST(NAME bn_quat_distance_2 COMMAND bn_test qmath 3 4,2,3,1 0,0,2,3 5)

# For function #4 (quat_double), the <args> format is as follows:
#
# q1 q2 expected_quat
#
# where q1, q2, and expected_quat are sequences of 4 comma-separated
# numbers each representing a quaternion (X, Y, Z, W).

BRLCAD_ADD_TEST(NAME bn_quat_double_0 COMMAND bn_test qmath 4 0,2,0,1 0,2,0,1 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_double_1 COMMAND bn_test qmath 4 0.548,0.365,0.730,0.183 0.753,0.282,0.564,0.188 0.900,0.177,0.355,0.179)

# For function #5 (quat_bisect), the <args> format is as follows:
#
# q1 q2 expected_quat
#
# where q1, q2, and expected_quat are sequences of 4 comma-separated
# numbers each representing a quaternion (X, Y, Z, W).

BRLCAD_ADD_TEST(NAME bn_quat_bisect_0 COMMAND bn_test qmath 5 0,0.894,0,0.447 0,0.894,0,0.447 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_bisect_1 COMMAND bn_test qmath 5 0.548,0.365,0.730,0.183 0.753,0.282,0.564,0.188 0.657,0.327,0.653,0.187)

# For function #6 (quat_slerp), the <args> format is as follows:
#
# q1 q2 f expected_quat
#
# where q1, q2, and expected_quat are sequences of 4 comma-separated
# numbers each representing a quaternion (X, Y, Z, W), and f is a
# single number which is the fraction between q1 and q2 to interpolate
# for.

BRLCAD_ADD_TEST(NAME bn_quat_slerp_same_0 COMMAND bn_test qmath 6 0,0.894,0,0.447 0,0.894,0,0.447 0 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_slerp_same_0.5 COMMAND bn_test qmath 6 0,0.894,0,0.447 0,0.894,0,0.447 0.5 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_slerp_same_0.783 COMMAND bn_test qmath 6 0,0.894,0,0.447 0,0.894,0,0.447 0.783 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_slerp_same_1 COMMAND bn_test qmath 6 0,0.894,0,0.447 0,0.894,0,0.447 1 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_slerp_diff_0 COMMAND bn_test qmath 6 0.548,0.365,0.730,0.183 0.753,0.282,0.564,0.188 0 0.548,0.365,0.730,0.183)
BRLCAD_ADD_TEST(NAME bn_quat_slerp_diff_0.5 COMMAND bn_test qmath 6 0.548,0.365,0.730,0.183 0.753,0.282,0.564,0.188 0.5 0.657,0.327,0.653,0.187)
BRLCAD_ADD_TEST(NAME bn_quat_slerp_diff_0.783 COMMAND bn_test qmath 6 0.548,0.365,0.730,0.183 0.753,0.282,0.564,0.188 0.783 0.713,0.302,0.604,0.188)
BRLCAD_ADD_TEST(NAME bn_quat_slerp_diff_1 COMMAND bn_test qmath 6 0.548,0.365,0.730,0.183 0.753,0.282,0.564,0.188 1 0.753,0.282,0.564,0.188)

# For function #7 (quat_sberp), the <args> format is as follows:
#
# q1 qa qb q2 f expected_quat
#
# where q1, qa, qb, q2, and expected_quat are sequences of 4
# comma-separated numbers representing a quaternion (X, Y, Z, W), and
# f is a single number which is the fraction between q1 and q2 to
# interpolate for.

BRLCAD_ADD_TEST(NAME bn_quat_sberp_same_0 COMMAND bn_test qmath 7 0,0.894,0,0.447 1,0,0,0 0.5,0.5,0.5,0.5  0,0.894,0,0.447 0 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_sberp_same_0.5 COMMAND bn_test qmath 7 0,0.894,0,0.447 1,0,0,0 0.5,0.5,0.5,0.5 0,0.894,0,0.447 0.5 0.723,0.529,0.230,0.380)
BRLCAD_ADD_TEST(NAME bn_quat_sberp_same_0.783 COMMAND bn_test qmath 7 0,0.894,0,0.447 1,0,0,0 0.5,0.5,0.5,0.5 0,0.894,0,0.447 0.783 0.391,0.740,0.241,0.491)
BRLCAD_ADD_TEST(NAME bn_quat_sberp_same_1 COMMAND bn_test qmath 7 0,0.894,0,0.447 1,0,0,0 0.5,0.5,0.5,0.5 0,0.894,0,0.447 1 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_sberp_diff_0 COMMAND bn_test qmath 7 0.548,0.365,0.730,0.183 1,0,0,0 0.5,0.5,0.5,0.5 0.753,0.282,0.564,0.188 0 0.548,0.365,0.730,0.183)
BRLCAD_ADD_TEST(NAME bn_quat_sberp_diff_0.5 COMMAND bn_test qmath 7 0.548,0.365,0.730,0.183 1,0,0,0 0.5,0.5,0.5,0.5 0.753,0.282,0.564,0.188 0.5 0.828,0.302,0.392,0.264)
BRLCAD_ADD_TEST(NAME bn_quat_sberp_diff_0.783 COMMAND bn_test qmath 7 0.548,0.365,0.730,0.183 1,0,0,0 0.5,0.5,0.5,0.5 0.753,0.282,0.564,0.188 0.783 0.724,0.358,0.501,0.310)
BRLCAD_ADD_TEST(NAME bn_quat_sberp_diff_1 COMMAND bn_test qmath 7 0.548,0.365,0.730,0.183 1,0,0,0 0.5,0.5,0.5,0.5 0.753,0.282,0.564,0.188 1 0.753,0.282,0.564,0.188)

# For function #8 (quat_make_nearest), the <args> format is as follows:
#
# q1 q2 f expected_result
#
# where q1, q2, and expected_result are sequences of 4 comma-separated
# numbers, each representing a quaternion (X, Y, Z, W).

BRLCAD_ADD_TEST(NAME bn_quat_make_nearest_same_0 COMMAND bn_test qmath 8 0,0.894,0,0.447 0,0.894,0,0.447 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_make_nearest_same_1 COMMAND bn_test qmath 8 0,-0.894,0,-0.447 0,0.894,0,0.447 0,0.894,0,0.447)
BRLCAD_ADD_TEST(NAME bn_quat_make_nearest_diff_0 COMMAND bn_test qmath 8 0.548,0.365,0.730,0.183 0.753,0.282,0.564,0.188 0.548,0.365,0.730,0.183)
BRLCAD_ADD_TEST(NAME bn_quat_make_nearest_diff_1 COMMAND bn_test qmath 8 -0.548,-0.365,-0.730,-0.183 0.753,0.282,0.564,0.188 0.548,0.365,0.730,0.183)

# For function #9 (quat_exp), the <args> format is as follows:
#
# in expected_out
#
# where in and expected_out are sequences of 4 comma-separated
# numbers, each representing a quaternion (X, Y, Z, W).

BRLCAD_ADD_TEST(NAME bn_quat_exp_0 COMMAND bn_test qmath 9 0,0,0,0 0,0,0,1)
BRLCAD_ADD_TEST(NAME bn_quat_exp_1 COMMAND bn_test qmath 9 0.577,0.577,0.577,0 0.486,0.486,0.486,0.541)
BRLCAD_ADD_TEST(NAME bn_quat_exp_2 COMMAND bn_test qmath 9 0.365,0.730,0.183,0 0.324,0.648,0.162,0.670)

# For function #10 (quat_exp), the <args> format is as follows:
#
# in expected_out
#
# where in and expected_out are sequences of 4 comma-separated
# numbers, each representing a quaternion (X, Y, Z, W).

BRLCAD_ADD_TEST(NAME bn_quat_log_0 COMMAND bn_test qmath 10 1,0,0,0 1.571,0,0,0)
BRLCAD_ADD_TEST(NAME bn_quat_log_1 COMMAND bn_test qmath 10 0.5,0.5,0.5,0.5 0.605,0.605,0.605,0)
BRLCAD_ADD_TEST(NAME bn_quat_log_2 COMMAND bn_test qmath 10 0.548,0.365,0.730,0.183 0.773,0.515,1.030,0)


#
#  *************** vlist.c ***************
#
# For vlist_cnt, input is as follows:
# vlist_cnt <args>

# the <args> format is as follows:
# expected_result
#
BRLCAD_ADD_TEST(NAME bn_vlist_cmd_cnt_NULL COMMAND bn_test vlist -1)
BRLCAD_ADD_TEST(NAME bn_vlist_cmd_cnt_0 COMMAND bn_test vlist 0)
BRLCAD_ADD_TEST(NAME bn_vlist_cmd_cnt_1 COMMAND bn_test vlist 5)
BRLCAD_ADD_TEST(NAME bn_vlist_cmd_cnt_15 COMMAND bn_test vlist 15)
BRLCAD_ADD_TEST(NAME bn_vlist_cmd_cnt_45 COMMAND bn_test vlist 45)
BRLCAD_ADD_TEST(NAME bn_vlist_cmd_cnt_500 COMMAND bn_test vlist 500)

#
#  *************** plot3.c ***************
#
#  Tests to detect validity/invalidity of plot3 files
#
BRLCAD_ADD_TEST(NAME bn_plot3_valid COMMAND bn_plot3 -b "${CMAKE_CURRENT_SOURCE_DIR}/valid.plot3")
BRLCAD_ADD_TEST(NAME bn_plot3_invalid COMMAND bn_plot3 -i -b "${CMAKE_CURRENT_SOURCE_DIR}/invalid.plot3")

CMAKEFILES(
  CMakeLists.txt
  bntester.dat
  bn_test.c.in
  poly_cubic_roots.c
  poly_quartic_roots.c
  valid.plot3
  invalid.plot3
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
