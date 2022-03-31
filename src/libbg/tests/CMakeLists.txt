
# To minimize the number of build targets and binaries that are created, we
# combine most (not all) of the unit tests into a single program.

set(bg_test_srcs
  plane_dist.c
  plane_isect.c
  plane_pt.c
  )

# Generate and assemble the necessary per-test-type source code
set(BG_TEST_SRC_INCLUDES)
set(BG_TEST_SRC_SWITCHES)
set(BG_TEST_LIST)
foreach(srcfile ${bg_test_srcs})
  get_filename_component(NAME_ROOT ${srcfile} NAME_WE)
  set(BG_TEST_SRC_INCLUDES "${BG_TEST_SRC_INCLUDES}\n#include \"libbg/tests/${srcfile}\"")
  set(BG_TEST_SRC_SWITCHES "${BG_TEST_SRC_SWITCHES}\n   if (BU_STR_EQUAL(av[0], \"${NAME_ROOT}\")) return ${NAME_ROOT}_main(ac,av);")
  set(BG_TEST_LIST "${BG_TEST_LIST}\\n  ${NAME_ROOT}")
endforeach(srcfile ${bg_test_srcs})

# Generate bg_test.c file and build it
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
configure_file(bg_test.c.in "${CMAKE_CURRENT_BINARY_DIR}/bg_test.c" @ONLY)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/bg_test.c")
BRLCAD_ADDEXEC(bg_test "${CMAKE_CURRENT_BINARY_DIR}/bg_test.c" libbg TEST)

# TODO - convert this into the scheme used by other tests below
BRLCAD_ADDEXEC(bgtester bgtester.c "libbu;libbn;libbg" TEST)



#  ************ obr.c tests *************

BRLCAD_ADDEXEC(bg_obr obr.c "libbg;libbn;libbu" TEST)

#BRLCAD_ADD_TEST(NAME bg_obr_simple			COMMAND bg_obr) # Default points

#
#  ************ plane.c tests *************

# For COMMAND bg_test plane_pt, the input format is as follows:
#
# COMMAND bg_test plane_pt <function number> <args>

# For function #1 (bg_3pts_collinear), the <args> format is as follows:
#
# P0 P1 P2 <expected result>
#
# where P0-P2 are the points in question. An individual point is three
# integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_false_1        COMMAND bg_test plane_pt 1  0,0,0 1,1,1 6,3,8 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_false_2        COMMAND bg_test plane_pt 1 0,0,0 6,3,8 1,1,1 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_false_3        COMMAND bg_test plane_pt 1 1,1,1 0,0,0 6,3,8 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_false_4        COMMAND bg_test plane_pt 1 1,1,1 6,3,8 0,0,0 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_false_5        COMMAND bg_test plane_pt 1 6,3,8 1,1,1 0,0,0  0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_false_6        COMMAND bg_test plane_pt 1 6,3,8 0,0,0 1,1,1 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_same           COMMAND bg_test plane_pt 1 5.3,8.1,9.6 5.3,8.1,9.6 5.3,8.1,9.6 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_partial_same_1 COMMAND bg_test plane_pt 1 6,2,4 6,2,4 9.3,8.1,4.6 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_partial_same_2 COMMAND bg_test plane_pt 1 9.3,8.1,4.6 6,2,4 6,2,4 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_partial_same_3 COMMAND bg_test plane_pt 1 6,2,4 9.3,8.1,4.6 6,2,4 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_true_1         COMMAND bg_test plane_pt 1 1.8,4.6,2.2 5.4,13.8,6.6 16.2,41.4,19.8 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_true_2         COMMAND bg_test plane_pt 1 1.8,4.6,2.2 16.2,41.4,19.8 5.4,13.8,6.6 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_true_3         COMMAND bg_test plane_pt 1 5.4,13.8,6.6 1.8,4.6,2.2 16.2,41.4,19.8 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_true_4         COMMAND bg_test plane_pt 1 5.4,13.8,6.6 16.2,41.4,19.8 1.8,4.6,2.2 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_true_5         COMMAND bg_test plane_pt 1 16.2,41.4,19.8 1.8,4.6,2.2 5.4,13.8,6.6 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_collinear_true_6         COMMAND bg_test plane_pt 1 16.2,41.4,19.8 5.4,13.8,6.6 1.8,4.6,2.2 1)

# For function #2 (bg_3pts_distinct), the <args> format is as follows:
#
#  P0 P1 P2 <expected result>
#
# where P0-P2 are the points in question. An individual point
# is three integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_false_all       COMMAND bg_test plane_pt 2 5.3,8.6,9.1 5.3,8.6,9.1 5.3,8.6,9.1 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_false_1         COMMAND bg_test plane_pt 2 5.3,8.6,9.1 5.3,8.6,9.1 8.7,4.1,9.3 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_false_2         COMMAND bg_test plane_pt 2 5.3,8.6,9.1 8.7,4.1,9.3 5.3,8.6,9.1 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_false_3         COMMAND bg_test plane_pt 2 8.7,4.1,9.3 5.3,8.6,9.1 5.3,8.6,9.1 0)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_true_1          COMMAND bg_test plane_pt 2 8.7,4.1,9.3 7.5,9.2,4.1 5.3,8.6,9.1 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_true_2          COMMAND bg_test plane_pt 2 8.7,4.1,9.3 5.3,8.6,9.1 7.5,9.2,4.1 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_true_3          COMMAND bg_test plane_pt 2 5.3,8.6,9.1 8.7,4.1,9.3 7.5,9.2,4.1 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_true_4          COMMAND bg_test plane_pt 2 5.3,8.6,9.1 7.5,9.2,4.1 8.7,4.1,9.3 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_true_5          COMMAND bg_test plane_pt 2 7.5,9.2,4.1 8.7,4.1,9.3 5.3,8.6,9.1 1)
BRLCAD_ADD_TEST(NAME bg_plane_3pts_distinct_true_6          COMMAND bg_test plane_pt 2 7.5,9.2,4.1 5.3,8.6,9.1 8.7,4.1,9.3 1)

# For function #3 (bg_distsq_line3_pt3), the <args> format is as follows:
#
#  PT DIR A <expected result>
#
# Where PT and DIR define the parametric line in question and A is the
# point in question.

BRLCAD_ADD_TEST(NAME bg_plane_distsq_line3_pt3_1            COMMAND bg_test plane_pt 3 0,0,0 1,1,1 0,0,0 0)
BRLCAD_ADD_TEST(NAME bg_plane_distsq_line3_pt3_2            COMMAND bg_test plane_pt 3 -1,-1,-1 1,1,1 0,0,0 0)
BRLCAD_ADD_TEST(NAME bg_plane_distsq_line3_pt3_3            COMMAND bg_test plane_pt 3 0,0,0 1,1,1 0,5,0 16.666666)
BRLCAD_ADD_TEST(NAME bg_plane_distsq_line3_pt3_4            COMMAND bg_test plane_pt 3 5,6,4 9,3,8 -5,2,4 48.441558)

# For function #4 (bg_distsq_pt3_lseg3_v2), the <args> format is as follows:
#
#  A B P <expected return> <expected distance>
#
# Where A and B are the endpoints of the line segment, and P is the
# point in question.

BRLCAD_ADD_TEST(NAME bg_plane_distsq_pt3_lseg3_v2_case0_1   COMMAND bg_test plane_pt 4 -5,-5,-5 5,5,5 0,0,0 0 0)
BRLCAD_ADD_TEST(NAME bg_plane_distsq_pt3_lseg3_v2_case1_1   COMMAND bg_test plane_pt 4 -5,-5,-5 5,5,5 -5,-5,-5 1 0)
BRLCAD_ADD_TEST(NAME bg_plane_distsq_pt3_lseg3_v2_case2_1   COMMAND bg_test plane_pt 4 -5,-5,-5 5,5,5 5,5,5 2 0)
BRLCAD_ADD_TEST(NAME bg_plane_distsq_pt3_lseg3_v2_case3_1   COMMAND bg_test plane_pt 4 -5,-5,-5 5,5,5 -10,-5,-10 3 50)
BRLCAD_ADD_TEST(NAME bg_plane_distsq_pt3_lseg3_v2_case4_1   COMMAND bg_test plane_pt 4 -5,-5,-5 5,5,5 5,10,10 4 50)
BRLCAD_ADD_TEST(NAME bg_plane_distsq_pt3_lseg3_v2_case5_1   COMMAND bg_test plane_pt 4 -5,-5,-5 5,5,5 0,-5,-5 5 16.666666)

# For function #5 (bg_mk_plane_3pts), the <args> format is as follows:
#
#  A B C <expected return
#
# Where A, B, and C are the points in question. This test function
# does not currently support checking the resultant plane.

BRLCAD_ADD_TEST(NAME bg_plane_mk_plane_3pts_fail_same        COMMAND bg_test plane_pt 5 0,0,0 0,0,0 0,0,0 -1)
BRLCAD_ADD_TEST(NAME bg_plane_mk_plane_3pts_fail_collinear_1 COMMAND bg_test plane_pt 5 0,0,0 5,8,2 0,0,0 -1)
BRLCAD_ADD_TEST(NAME bg_plane_mk_plane_3pts_fail_collinear_2 COMMAND bg_test plane_pt 5 5,8,2 0,0,0 0,0,0 -1)
BRLCAD_ADD_TEST(NAME bg_plane_mk_plane_3pts_fail_collinear_3 COMMAND bg_test plane_pt 5 -10,-10,-10 0,0,0 5,5,5 -1)
BRLCAD_ADD_TEST(NAME bg_plane_mk_plane_3pts_succeed          COMMAND bg_test plane_pt 5 -10,-10,-10 7,3,8 19,4,2 0)

# For function #6 (bg_plane_closest_pt), the <args> format is as follows:
#
#  A,B,C,D Px,Py,Pz
#
#  A-D are plane equation parameters, and P is the test point.
BRLCAD_ADD_TEST(NAME bg_plane_pt_project  COMMAND bg_test plane_pt 6 1,2,3,4 5.345220032350307,-3.124899935299395,-4.687349902949093)

# For function #7 (bg_coplanar_pts), the <args> format is as follows:
#
# P1x,P1y,P1z P2x,P2y,P2z P3x,P3y,P3z P4x,P4y,P4z [0,1]
#
# 0 indicates the points are not expected to test coplanar, 1 indicates they
# should test coplanar.
BRLCAD_ADD_TEST(NAME bg_plane_pts_coplanar_1  COMMAND bg_test plane_pt 7 1,0,0 0,1,0 1,1,0 2,2,0 1)
BRLCAD_ADD_TEST(NAME bg_plane_pts_coplanar_2  COMMAND bg_test plane_pt 7 1,0,0 0,1,0 1,1,0 2,2,1 0)
BRLCAD_ADD_TEST(NAME bg_plane_pts_coplanar_3  COMMAND bg_test plane_pt 7
-3709.082985807021941582207,3510.525439930336233373964,3905.461506563813145476161
-3152.697964017485901422333,2183.289655637726355053019,5294.303067153545271139592
-5625.550060079097420384642,3027.217726896207295794738,4211.349734112171063316055
-5069.165038289564108708873,1699.981942603596962726442,5600.191294701904553221539
1)
BRLCAD_ADD_TEST(NAME bg_plane_pts_coplanar_4  COMMAND bg_test plane_pt 7
-3709.082985807021941582207,3510.525439930336233373964,3905.461506563813145476161
-3152.697964017485901422333,2183.289655637726355053019,5294.303067153545271139592
-5625.550060079097420384642,3027.217726896207295794738,4211.349734112171063316055
-4019.727,2883.893,4507.108
1)
BRLCAD_ADD_TEST(NAME bg_plane_pts_coplanar_5  COMMAND bg_test plane_pt 7
-3709.082985807021941582207,3510.525439930336233373964,3905.461506563813145476161
-3152.697964017485901422333,2183.289655637726355053019,5294.303067153545271139592
-5625.550060079097420384642,3027.217726896207295794738,4211.349734112171063316055
-4000,2000,4000
0)

# For COMMAND bg_test plane_dist, the input format is as follows:
#
# COMMAND bg_test plane_dist <function number> <args>

# For function #1 (bg_dist_pt3_line3), the <args> format is as follows:
#
# Ax,Ay,Az DIRx,DIRy,DIRz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz
#
# where A and DIR define the line segment, and P is the point in
# question. An individual point is three integer or floating point
# numbers separated by commas.

BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_line3_case0_1   COMMAND bg_test plane_dist 2 -5,-5,-5 1,1,1 -5,-5,-5 0 0 -5,-5,-5)
BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_line3_case1_1   COMMAND bg_test plane_dist 2 -5,-5,-5 1,1,1 0,0,0 1 0.5 0,0,0)
BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_line3_case2_1   COMMAND bg_test plane_dist 2 -5,-5,-5 5,5,5 0,-5,-5 2 4.08248 -3.33333,-3.33333,-3.33333)

# For function #2 (bg_dist_pt3_lseg3), the <args> format is as follows:
#
# Ax,Ay,Az Bx,By,Bz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz
#
# where A and B define the line segment, and P is the point in
# question. An individual point is three integer or floating point
# numbers separated by commas.

BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_lseg3_case0_1   COMMAND bg_test plane_dist 2 -5,-5,-5 5,5,5 0,0,0 0 0.5 0,0,0)
BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_lseg3_case1_1   COMMAND bg_test plane_dist 2 -5,-5,-5 5,5,5 -5,-5,-5 1 0 -5,-5,-5)
BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_lseg3_case2_1   COMMAND bg_test plane_dist 2 -5,-5,-5 5,5,5 5,5,5 2 0 5,5,5)
BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_lseg3_case3_1   COMMAND bg_test plane_dist 2 -5,-5,-5 5,5,5 -10,-5,-10 3 7.07107 -5,-5,-5)
BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_lseg3_case4_1   COMMAND bg_test plane_dist 2 -5,-5,-5 5,5,5 5,10,10 4 7.07107 5,5,5)
BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_lseg3_case5_1   COMMAND bg_test plane_dist 2 -5,-5,-5 5,5,5 0,-5,-5 5 4.08248 -3.33333,-3.33333,-3.33333)

# For function #3 (bg_dist_pt3_pt3), the <args> format is as follows:
#
# Ax,Ay,Az Bx,By,Bz expected_return
#
# where A and B are the points in question. An individual point is
# three integer or floating point numbers separated by commas.

BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_pt3_same   COMMAND bg_test plane_dist 3 0,0,0 0,0,0 0)
BRLCAD_ADD_TEST(NAME bg_plane_dist_pt3_pt3_1      COMMAND bg_test plane_dist 3 0,0,0 1,1,1 1.732051)

# For COMMAND bg_test plane_isect, the input format is as follows:
#
# COMMAND bg_test plane_isect <function number> <args>

# For function #1 (bg_isect_line_lseg), the <args> format is as follows:
#
# Px,Py,Pz Dx,Dy,Dz Ax,Ay,Az Bx,By,Bz expected_return expected_t
#
# where P and D define the parametric line, and A and B define the
# line segment. An individual point is three integer or floating point
# numbers separated by commas.

BRLCAD_ADD_TEST(NAME bg_plane_isect_line_lseg_case-4_1   COMMAND bg_test plane_isect 1 -5,-5,-5 1,1,1 0,0,0 0,0,0 -4 0)
BRLCAD_ADD_TEST(NAME bg_plane_isect_line_lseg_case-3_1   COMMAND bg_test plane_isect 1 -5,-5,-5 1,1,1 0,0,1 0,0,5 -3 8.660)
BRLCAD_ADD_TEST(NAME bg_plane_isect_line_lseg_case-2_1   COMMAND bg_test plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,-1 -2 8.660)
BRLCAD_ADD_TEST(NAME bg_plane_isect_line_lseg_case-1_1   COMMAND bg_test plane_isect 1 -5,-5,-5 1,1,1 -5,-5,-6 0,0,-1 -1 0)
BRLCAD_ADD_TEST(NAME bg_plane_isect_line_lseg_case0_1    COMMAND bg_test plane_isect 1 -5,-5,-5 1,1,1 1,1,1 5,5,5 0 10.392)
BRLCAD_ADD_TEST(NAME bg_plane_isect_line_lseg_case1_1    COMMAND bg_test plane_isect 1 -5,-5,-5 1,1,1 0,0,0 0,0,5 1 8.660)
BRLCAD_ADD_TEST(NAME bg_plane_isect_line_lseg_case2_1    COMMAND bg_test plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,0 2 8.660)
BRLCAD_ADD_TEST(NAME bg_plane_isect_line_lseg_case3_1    COMMAND bg_test plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,5 3 8.660)

# For function #2 (bg_isect_lseg3_lseg3), the <args> format is as follows:
#
# Px,Py,Pz PDIRx,PDIRy,PDIRz Qx,Qy,Qz QDIRx,QDIRy,QDIRz expected_return expected_dist_0 expected_dist_1
#
# Where P and PDIR define the first segment, and Q and QDIR define the
# second. expected_dist_0 and expected_dist_1 are the expected values
# of the two components of the dist[] array output by the function. An
# individual point is three integer or floating point numbers
# separated by commas.

BRLCAD_ADD_TEST(NAME bg_plane_isect_lseg3_lseg3_case-3_1   COMMAND bg_test plane_isect 2 0,0,0 0,1,1 0,0,1 0,-1,1 -3 0.5 -0.5)
BRLCAD_ADD_TEST(NAME bg_plane_isect_lseg3_lseg3_case-3_2   COMMAND bg_test plane_isect 2 0,0,0 0,1,1 0,1,0 0,-0.25,0.25 -3 0.5 2)
BRLCAD_ADD_TEST(NAME bg_plane_isect_lseg3_lseg3_case-2_1   COMMAND bg_test plane_isect 2 0,0,0 0,1,1 1,1,1 0,1,1 -2 0 0)
BRLCAD_ADD_TEST(NAME bg_plane_isect_lseg3_lseg3_case-1_1   COMMAND bg_test plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -1,-1,-1 -1 1.666667 1.333333)
BRLCAD_ADD_TEST(NAME bg_plane_isect_lseg3_lseg3_case0_1    COMMAND bg_test plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -2,-2,-2 0 1.666667 1)
BRLCAD_ADD_TEST(NAME bg_plane_isect_lseg3_lseg3_case0_2    COMMAND bg_test plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -3,-3,-3 0 1.666667 0.666667)
BRLCAD_ADD_TEST(NAME bg_plane_isect_lseg3_lseg3_case0_3    COMMAND bg_test plane_isect 2 -5,-5,-5 5,5,5 -4,-4,-4 2,2,2 0 0.2 0.6)
BRLCAD_ADD_TEST(NAME bg_plane_isect_lseg3_lseg3_case1_1    COMMAND bg_test plane_isect 2 0,0,0 0,1,1 0,1,0 0,-1,1 1 0.5 0.5)


#  ************ polygon.c tests ***********
BRLCAD_ADDEXEC(bg_polygon_triangulate polygon_triangulate.c "libbg;libbn;libbu" TEST)

BRLCAD_ADD_TEST(NAME bg_polygon_triangulate	    COMMAND bg_polygon_triangulate)


#  ************ chull.c tests ***********
# 2D Convex Hull tests
BRLCAD_ADDEXEC(bg_chull chull.c "libbg;libbn;libbu" TEST)
CMAKEFILES(chull_test003_bot.asc)
CMAKEFILES(chull_test004_bot.asc)

#  ************ chull3d.cpp tests ***********

# 3D Convex Hull tests
BRLCAD_ADDEXEC(bg_chull3d chull3d.c "libbg;libbn;libbu" TEST)

#  ************ lseg_lseg.c tests ***********

BRLCAD_ADDEXEC(bg_lseg_lseg_dist lseg_lseg.c "libbg;libbn;libbu" TEST)

BRLCAD_ADD_TEST(NAME bg_lseg_lseg_1   COMMAND bg_lseg_lseg_dist 1)

#  ************ lseg_lseg.c tests ***********

BRLCAD_ADDEXEC(bg_lseg_pt_dist lseg_pt.c "libbg;libbn;libbu" TEST)
BRLCAD_ADD_TEST(NAME bg_lseg_pt_dist_case0  COMMAND bg_lseg_pt_dist  -5,-5,-5 5,5,5 0,0,0 0,0,0 0)
BRLCAD_ADD_TEST(NAME bg_lseg_pt_dist_case1  COMMAND bg_lseg_pt_dist  -5,-5,-5 5,5,5 -5,-5,-5 -5,-5,-5, 0)
BRLCAD_ADD_TEST(NAME bg_lseg_pt_dist_case2  COMMAND bg_lseg_pt_dist  -5,-5,-5 5,5,5 5,5,5 5,5,5, 0)
BRLCAD_ADD_TEST(NAME bg_lseg_pt_dist_case3  COMMAND bg_lseg_pt_dist  -5,-5,-5 5,5,5 -10,-5,-10 -5,-5,-5 7.07107 )
BRLCAD_ADD_TEST(NAME bg_lseg_pt_dist_case4  COMMAND bg_lseg_pt_dist  -5,-5,-5 5,5,5 5,10,10 5,5,5 7.07107)
BRLCAD_ADD_TEST(NAME bg_lseg_pt_dist_case5  COMMAND bg_lseg_pt_dist  -5,-5,-5 5,5,5 0,-5,-5 -3.33333,-3.33333,-3.33333 4.08248)

#  ************ tri_ray.c tests ***********

BRLCAD_ADDEXEC(bg_tri_ray_isect tri_ray_isect.cpp "libbg;libbn;libbu" TEST)


#  ************ tri_tri.c tests ***********

# For COMMAND bg_tri_tri_isect_coplanar, the input format is as follows:
#
# COMMAND bg_tri_tri_isect_coplanar V0 V1 V2 U0 U1 U2 <area_flag> <expected result>
#
# where P and Q are the two triangles in question.  An individual point
# is three integer or floating point numbers separated by commas. The area
# flag tells the function whether or not to require non-zero area for an
# overlap in coplanar cases.

BRLCAD_ADDEXEC(bg_tri_tri_isect_coplanar tri_tri_isect_coplanar.cpp "libbg;libbn;libbu" TEST)

# TODO - need some tests with floating point vertices that are down around the EPSILON threshold - that's
# where the NEAR_ZERO components of the bg_tri_tri_isect_coplanar logic become important.

BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_null_noarea   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  0  0)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_vertex_noarea COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        0  1)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_edge_noarea   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        0  1)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_full_noarea   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    0  1)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_null_area   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  1  0)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_vertex_area COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        1  0)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_edge_area   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        1  0)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_full_area   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    1  1)

# TODO - in the debugger this is looking like it shouldn't return intersecting - it's very nearly an edge only intersect - but the final point-in-triangle test is doing something I don't quite understand: it seems to be checking only one point per triangle for inside/outside instead of all three points against each triangle...
#BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_coplanar_f1 COMMAND bg_tri_tri_isect_coplanar -0.17062162871133435,0.02417062256398481,0.00000000000000000 -0.20768111931464361,-0.34362144340377737,0.00000000000000000 0.17876567813733310,-0.05826268893839849,0.00000000000000000 0.17876567813748631,-0.05826268893840295,0.00000000000000000 -0.17062162871171396,0.02417062256397554,0.00000000000000000 0.19139302046287024,0.41180557615261132,0.00000000000000000 1 0)

# For COMMAND bg_tri_tri_isect, the input format is as follows:
#
# COMMAND bg_tri_tri_isect V0 V1 V2 U0 U1 U2 <expected result>
#
# where P and Q are the two triangles in question.  An individual point
# is three integer or floating point numbers separated by commas.

BRLCAD_ADDEXEC(bg_tri_tri_isect tri_tri_isect.c "libbg;libbn;libbu" TEST)

# Test coplanar triangles
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_cp_null   COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  0)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_cp_vertex COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        1)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_cp_edge   COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        1)
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_cp_full   COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    1)

# Test more general triangles
BRLCAD_ADD_TEST(NAME bg_tri_tri_isect_null   COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,1 1,0,1 1,1,1  0)


#  ************ tri_closest_pt.c tests ***********

BRLCAD_ADDEXEC(bg_tri_closest_pt tri_closest_pt.c "libbg;libbn;libbu" TEST)

BRLCAD_ADD_TEST(NAME bg_tri_pt_dist_coplanar_center   COMMAND bg_tri_closest_pt  0,0,0 -1,-1,0 1,-1,0 0,1,0 0)
BRLCAD_ADD_TEST(NAME bg_tri_pt_dist_coplanar_vert_closest   COMMAND bg_tri_closest_pt -2,-1,0 -1,-1,0 1,-1,0 0,1,0 1)


#  ************ polygon_op.cpp tests ***********

BRLCAD_ADDEXEC(bg_polygon_op polygon_op.c "libbg;libbn;libbu" TEST)

BRLCAD_ADD_TEST(NAME bg_polygon_op_basic   COMMAND bg_polygon_op)

#  ************ sat.cpp Oriented Bounding Box intersection tests ***********

BRLCAD_ADDEXEC(bg_sat sat.c "libbg;libbn;libbu" TEST)

BRLCAD_ADD_TEST(NAME bg_sat_basic   COMMAND bg_sat)


#  ************ trimesh_sync.cpp tests ***********

BRLCAD_ADDEXEC(bg_trimesh_sync trimesh_sync.c "libbg;libbn;libbu" TEST)

BRLCAD_ADD_TEST(NAME bg_trimesh_sync  COMMAND bg_trimesh_sync)


CMAKEFILES(
  bg_test.c.in
  plane_dist.c
  plane_isect.c
  plane_pt.c
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
