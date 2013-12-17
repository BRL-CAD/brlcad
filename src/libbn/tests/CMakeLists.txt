BRLCAD_ADDEXEC(tester_bn_tri_tri_isect_coplanar bn_tri_tri_isect_coplanar.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_tri_tri_isect bn_tri_tri_isect.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_list bn_list.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_poly_multiply bn_poly_multiply.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_poly_scale bn_poly_scale.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_poly_add bn_poly_add.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_poly_sub bn_poly_sub.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_poly_synthetic_div bn_poly_synthetic_div.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_obr bn_obr.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_chull bn_chull.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_plane_pt bn_plane_pt.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_plane_dist bn_plane_dist.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_plane_isect bn_plane_isect.c "libbu;libbn" NO_INSTALL)
CMAKEFILES(bn_chull_test003_bot.asc)
CMAKEFILES(bn_chull_test004_bot.asc)
CMAKEFILES(bn_poly_cubic_roots.c bn_poly_quartic_roots.c bn_obr.c)

# For tester_bn_tri_tri_isect_coplanar, the input format is as follows:
#
# tester_bn_tri_tri_isect_coplanar V0 V1 V2 U0 U1 U2 <area_flag> <expected result>
#
# where P and Q are the two triangles in question.  An individual point
# is three integer or floating point numbers separated by commas. The area
# flag tells the function whether or not to require non-zero area for an
# overlap in coplanar cases.

# TODO - need some tests with floating point vertices that are down around the EPSILON threshold - that's
# where the NEAR_ZERO components of the bn_tri_tri_isect_coplanar logic become important.

add_test(bn_tri_tri_isect_coplanar_null_noarea   tester_bn_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  0  0)
add_test(bn_tri_tri_isect_coplanar_vertex_noarea tester_bn_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        0  1)
add_test(bn_tri_tri_isect_coplanar_edge_noarea   tester_bn_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        0  1)
add_test(bn_tri_tri_isect_coplanar_full_noarea   tester_bn_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    0  1)
add_test(bn_tri_tri_isect_coplanar_null_area   tester_bn_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  1  0)
add_test(bn_tri_tri_isect_coplanar_vertex_area tester_bn_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        1  0)
add_test(bn_tri_tri_isect_coplanar_edge_area   tester_bn_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        1  0)
add_test(bn_tri_tri_isect_coplanar_full_area   tester_bn_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    1  1)

# For tester_bn_tri_tri_isect, the input format is as follows:
#
# tester_bn_tri_tri_isect V0 V1 V2 U0 U1 U2 <expected result>
#
# where P and Q are the two triangles in question.  An individual point
# is three integer or floating point numbers separated by commas.

# Test coplanar triangles
add_test(bn_tri_tri_isect_cp_null   tester_bn_tri_tri_isect  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  0)
add_test(bn_tri_tri_isect_cp_vertex tester_bn_tri_tri_isect  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        1)
add_test(bn_tri_tri_isect_cp_edge   tester_bn_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        1)
add_test(bn_tri_tri_isect_cp_full   tester_bn_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    1)

# Test more general triangles
add_test(bn_tri_tri_isect_null   tester_bn_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,1 1,0,1 1,1,1  0)


#
#  ************ list.c tests *************
#
# Format is:
#  tester_bn_list  <2 or 3 (the dimension)>  X0,Y0[,Z0] X1,Y1[,Z1] Xn,Yn[,Zn]
add_test(bn_list_2d_null                tester_bn_list  2) # NULL list of points
add_test(bn_list_2d_1                   tester_bn_list  2  0,0)
add_test(bn_list_2d_2                   tester_bn_list  2  0,0 1.64,1)
add_test(bn_list_2d_3                   tester_bn_list  2  3,7 1,-5)
add_test(bn_list_2d_4                   tester_bn_list  2  3,7 1,-5.321 67,0 0,100)


add_test(bn_list_3d_null                tester_bn_list  3) # NULL list of points
add_test(bn_list_3d_1                   tester_bn_list  3  0,0,0)
add_test(bn_list_3d_2                   tester_bn_list  3  0,0,3 1.64,1,4)
add_test(bn_list_3d_3                   tester_bn_list  3  3,7,8 1,-5,8)
add_test(bn_list_3d_4                   tester_bn_list  3  3,7,8 1,-5.321,3 67,0,67 0,100,23)


#
#  *********** poly.c tests *************
#
#Format is:
# tester_bn_poly      <  void      >         void
add_test(bn_poly_multiply              tester_bn_poly_multiply)
add_test(bn_poly_scale                 tester_bn_poly_scale)
add_test(bn_poly_add                   tester_bn_poly_add)

# failure encountered, no output to indicate why (disabled in prep for release)
# add_test(bn_poly_sub                   tester_bn_poly_sub)

add_test(bn_poly_synthetic_div               tester_bn_poly_synthetic_div)

#
#  ************ obr.c tests *************
#add_test(bn_obr_simple			tester_bn_obr) # Default points

#
#  ************ plane.c tests *************

# For tester_bn_plane_pt, the input format is as follows:
#
# tester_bn_plane_pt <function number> <args>

# For function #1 (bn_3pts_collinear), the <args> format is as follows:
#
# P0 P1 P2 <expected result>
#
# where P0-P2 are the points in question. An individual point is three
# integer or floating point numbers separated by commas.

add_test(bn_plane_3pts_collinear_false_1        tester_bn_plane_pt 1  0,0,0 1,1,1 6,3,8 0)
add_test(bn_plane_3pts_collinear_false_2        tester_bn_plane_pt 1 0,0,0 6,3,8 1,1,1 0)
add_test(bn_plane_3pts_collinear_false_3        tester_bn_plane_pt 1 1,1,1 0,0,0 6,3,8 0)
add_test(bn_plane_3pts_collinear_false_4        tester_bn_plane_pt 1 1,1,1 6,3,8 0,0,0 0)
add_test(bn_plane_3pts_collinear_false_5        tester_bn_plane_pt 1 6,3,8 1,1,1 0,0,0  0)
add_test(bn_plane_3pts_collinear_false_6        tester_bn_plane_pt 1 6,3,8 0,0,0 1,1,1 0)
add_test(bn_plane_3pts_collinear_same           tester_bn_plane_pt 1 5.3,8.1,9.6 5.3,8.1,9.6 5.3,8.1,9.6 1)
add_test(bn_plane_3pts_collinear_partial_same_1 tester_bn_plane_pt 1 6,2,4 6,2,4 9.3,8.1,4.6 1)
# This test fails, but I think that it shouldn't --- it seems to be
# showing that point order matters since it is the same three points
# as the preceding and following tests, but makes a different result.
add_test(bn_plane_3pts_collinear_partial_same_2 tester_bn_plane_pt 1 9.3,8.1,4.6 6,2,4 6,2,4 1)
add_test(bn_plane_3pts_collinear_partial_same_3 tester_bn_plane_pt 1 6,2,4 9.3,8.1,4.6 6,2,4 1)
add_test(bn_plane_3pts_collinear_true_1         tester_bn_plane_pt 1 1.8,4.6,2.2 5.4,13.8,6.6 16.2,41.4,19.8 1)
add_test(bn_plane_3pts_collinear_true_2         tester_bn_plane_pt 1 1.8,4.6,2.2 16.2,41.4,19.8 5.4,13.8,6.6 1)
add_test(bn_plane_3pts_collinear_true_3         tester_bn_plane_pt 1 5.4,13.8,6.6 1.8,4.6,2.2 16.2,41.4,19.8 1)
add_test(bn_plane_3pts_collinear_true_4         tester_bn_plane_pt 1 5.4,13.8,6.6 16.2,41.4,19.8 1.8,4.6,2.2 1)
add_test(bn_plane_3pts_collinear_true_5         tester_bn_plane_pt 1 16.2,41.4,19.8 1.8,4.6,2.2 5.4,13.8,6.6 1)
add_test(bn_plane_3pts_collinear_true_6         tester_bn_plane_pt 1 16.2,41.4,19.8 5.4,13.8,6.6 1.8,4.6,2.2 1)

# For function #2 (bn_3pts_distinct), the <args> format is as follows:
#
#  P0 P1 P2 <expected result>
#
# where P0-P2 are the points in question. An individual point
# is three integer or floating point numbers separated by commas.

add_test(bn_plane_3pts_distinct_false_all       tester_bn_plane_pt 2 5.3,8.6,9.1 5.3,8.6,9.1 5.3,8.6,9.1 0)
add_test(bn_plane_3pts_distinct_false_1         tester_bn_plane_pt 2 5.3,8.6,9.1 5.3,8.6,9.1 8.7,4.1,9.3 0)
add_test(bn_plane_3pts_distinct_false_2         tester_bn_plane_pt 2 5.3,8.6,9.1 8.7,4.1,9.3 5.3,8.6,9.1 0)
add_test(bn_plane_3pts_distinct_false_3         tester_bn_plane_pt 2 8.7,4.1,9.3 5.3,8.6,9.1 5.3,8.6,9.1 0)
add_test(bn_plane_3pts_distinct_true_1          tester_bn_plane_pt 2 8.7,4.1,9.3 7.5,9.2,4.1 5.3,8.6,9.1 1)
add_test(bn_plane_3pts_distinct_true_2          tester_bn_plane_pt 2 8.7,4.1,9.3 5.3,8.6,9.1 7.5,9.2,4.1 1)
add_test(bn_plane_3pts_distinct_true_3          tester_bn_plane_pt 2 5.3,8.6,9.1 8.7,4.1,9.3 7.5,9.2,4.1 1)
add_test(bn_plane_3pts_distinct_true_4          tester_bn_plane_pt 2 5.3,8.6,9.1 7.5,9.2,4.1 8.7,4.1,9.3 1)
add_test(bn_plane_3pts_distinct_true_5          tester_bn_plane_pt 2 7.5,9.2,4.1 8.7,4.1,9.3 5.3,8.6,9.1 1)
add_test(bn_plane_3pts_distinct_true_6          tester_bn_plane_pt 2 7.5,9.2,4.1 5.3,8.6,9.1 8.7,4.1,9.3 1)

# For function #3 (bn_distsq_line3_pt3), the <args> format is as follows:
#
#  PT DIR A <expected result>
#
# Where PT and DIR define the parametric line in question and A is the
# point in question.

add_test(bn_plane_distsq_line3_pt3_1            tester_bn_plane_pt 3 0,0,0 1,1,1 0,0,0 0)
add_test(bn_plane_distsq_line3_pt3_2            tester_bn_plane_pt 3 -1,-1,-1 1,1,1 0,0,0 0)
add_test(bn_plane_distsq_line3_pt3_3            tester_bn_plane_pt 3 0,0,0 1,1,1 0,5,0 16.666666)
add_test(bn_plane_distsq_line3_pt3_4            tester_bn_plane_pt 3 5,6,4 9,3,8 -5,2,4 48.441558)

# For function #4 (bn_distsq_pt3_lseg3_v2), the <args> format is as follows:
#
#  A B P <expected return> <expected distance>
#
# Where A and B are the endpoints of the line segment, and P is the
# point in question.

add_test(bn_plane_distsq_pt3_lseg3_v2_case0_1   tester_bn_plane_pt 4 -5,-5,-5 5,5,5 0,0,0 0 0)
add_test(bn_plane_distsq_pt3_lseg3_v2_case1_1   tester_bn_plane_pt 4 -5,-5,-5 5,5,5 -5,-5,-5 1 0)
add_test(bn_plane_distsq_pt3_lseg3_v2_case2_1   tester_bn_plane_pt 4 -5,-5,-5 5,5,5 5,5,5 2 0)
add_test(bn_plane_distsq_pt3_lseg3_v2_case3_1   tester_bn_plane_pt 4 -5,-5,-5 5,5,5 -10,-5,-10 3 50)
add_test(bn_plane_distsq_pt3_lseg3_v2_case4_1   tester_bn_plane_pt 4 -5,-5,-5 5,5,5 5,10,10 4 50)
add_test(bn_plane_distsq_pt3_lseg3_v2_case5_1   tester_bn_plane_pt 4 -5,-5,-5 5,5,5 0,-5,-5 5 16.666666)

# For function #5 (bn_mk_plane_3pts), the <args> format is as follows:
#
#  A B C <expected return
#
# Where A, B, and C are the points in question. This test function
# does not currently support checking the resultant plane.

add_test(bn_plane_mk_plane_3pts_fail_same       tester_bn_plane_pt 5 0,0,0 0,0,0 0,0,0 -1)
add_test(bn_plane_mk_plane_3pts_fail_colinear_1 tester_bn_plane_pt 5 0,0,0 5,8,2 0,0,0 -1)
add_test(bn_plane_mk_plane_3pts_fail_colinear_2 tester_bn_plane_pt 5 5,8,2 0,0,0 0,0,0 -1)
add_test(bn_plane_mk_plane_3pts_fail_colinear_3 tester_bn_plane_pt 5 -10,-10,-10 0,0,0 5,5,5 -1)
add_test(bn_plane_mk_plane_3pts_succeed         tester_bn_plane_pt 5 -10,-10,-10 7,3,8 19,4,2 0)


# For tester_bn_plane_dist, the input format is as follows:
#
# tester_bn_plane_dist <function number> <args>

# For function #1 (bn_dist_pt3_line3), the <args> format is as follows:
#
# Ax,Ay,Az DIRx,DIRy,DIRz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz
#
# where A and DIR define the line segment, and P is the point in
# question. An individual point is three integer or floating point
# numbers separated by commas.

add_test(bn_plane_dist_pt3_line3_case0_1   tester_bn_plane_dist 2 -5,-5,-5 1,1,1 -5,-5,-5 0 0 -5,-5,-5)
add_test(bn_plane_dist_pt3_line3_case1_1   tester_bn_plane_dist 2 -5,-5,-5 1,1,1 0,0,0 1 0.5 0,0,0)
add_test(bn_plane_dist_pt3_line3_case2_1   tester_bn_plane_dist 2 -5,-5,-5 5,5,5 0,-5,-5 2 4.08248 -3.33333,-3.33333,-3.33333)

# For function #2 (bn_dist_pt3_lseg3), the <args> format is as follows:
#
# Ax,Ay,Az Bx,By,Bz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz
#
# where A and B define the line segment, and P is the point in
# question. An individual point is three integer or floating point
# numbers separated by commas.

add_test(bn_plane_dist_pt3_lseg3_case0_1   tester_bn_plane_dist 2 -5,-5,-5 5,5,5 0,0,0 0 0.5 0,0,0)
add_test(bn_plane_dist_pt3_lseg3_case1_1   tester_bn_plane_dist 2 -5,-5,-5 5,5,5 -5,-5,-5 1 0 -5,-5,-5)
add_test(bn_plane_dist_pt3_lseg3_case2_1   tester_bn_plane_dist 2 -5,-5,-5 5,5,5 5,5,5 2 0 5,5,5)
add_test(bn_plane_dist_pt3_lseg3_case3_1   tester_bn_plane_dist 2 -5,-5,-5 5,5,5 -10,-5,-10 3 7.07107 -5,-5,-5)
add_test(bn_plane_dist_pt3_lseg3_case4_1   tester_bn_plane_dist 2 -5,-5,-5 5,5,5 5,10,10 4 7.07107 5,5,5)
add_test(bn_plane_dist_pt3_lseg3_case5_1   tester_bn_plane_dist 2 -5,-5,-5 5,5,5 0,-5,-5 5 4.08248 -3.33333,-3.33333,-3.33333)

# For function #3 (bn_dist_pt3_pt3), the <args> format is as follows:
#
# Ax,Ay,Az Bx,By,Bz expected_return
#
# where A and B are the points in question. An individual point is
# three integer or floating point numbers separated by commas.

add_test(bn_plane_dist_pt3_pt3_same   tester_bn_plane_dist 3 0,0,0 0,0,0 0)
add_test(bn_plane_dist_pt3_pt3_1      tester_bn_plane_dist 3 0,0,0 1,1,1 1.732051)

# For tester_bn_plane_isect, the input format is as follows:
#
# tester_bn_plane_isect <function number> <args>

# For function #1 (bn_isect_line_lseg), the <args> format is as follows:
#
# Px,Py,Pz Dx,Dy,Dz Ax,Ay,Az Bx,By,Bz expected_return expected_t
#
# where P and D define the parametric line, and A and B define the
# line segment. An individual point is three integer or floating point
# numbers separated by commas.

add_test(bn_plane_isect_line_lseg_case-4_1   tester_bn_plane_isect 1 -5,-5,-5 1,1,1 0,0,0 0,0,0 -4 0)
# This test fails because it thinks that there is an intersection
# between a and b, even though the intersection is less than a.
add_test(bn_plane_isect_line_lseg_case-3_1   tester_bn_plane_isect 1 -5,-5,-5 1,1,1 0,0,1 0,0,5 -3 6)
# This test fails because it does not return the correct t.
add_test(bn_plane_isect_line_lseg_case-2_1   tester_bn_plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,1 -2 6)
add_test(bn_plane_isect_line_lseg_case-1_1   tester_bn_plane_isect 1 -5,-5,-5 1,1,1 -5,-5,-6 0,0,-1 -1 0)
# This test fails because it does not return the correct t for a
add_test(bn_plane_isect_line_lseg_case0_1    tester_bn_plane_isect 1 -5,-5,-5 1,1,1 1,1,1 5,5,5 0 6)
# This test fails because it thinks that there is an intersection
# between a and be even though they intersect at a
add_test(bn_plane_isect_line_lseg_case1_1    tester_bn_plane_isect 1 -5,-5,-5 1,1,1 0,0,0 0,0,5 1 5)
# This test fails because it thinks that the intersection is past b
# even though it is at b
add_test(bn_plane_isect_line_lseg_case2_1    tester_bn_plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,0 2 5)
# This test fails because it does not return the correct t for the intersection
add_test(bn_plane_isect_line_lseg_case3_1    tester_bn_plane_isect 1 -5,-5,-5 1,1,1 0,0,-5 0,0,5 3 5)

# For function #2 (bn_isect_lseg3_lseg3), the <args> format is as follows:
#
# Px,Py,Pz PDIRx,PDIRy,PDIRz Qx,Qy,Qz QDIRx,QDIRy,QDIRz expected_return expected_dist_0 expected_dist_1
#
# Where P and PDIR define the first segment, and Q and QDIR define the
# second. expected_dist_0 and expected_dist_1 are the expected values
# of the two components of the dist[] array output by the function. An
# individual point is three integer or floating point numbers
# separated by commas.

add_test(bn_plane_isect_lseg3_lseg3_case-3_1   tester_bn_plane_isect 2 0,0,0 0,1,1 0,0,1 0,-1,1 -3 0.5 -0.5)
add_test(bn_plane_isect_lseg3_lseg3_case-3_2   tester_bn_plane_isect 2 0,0,0 0,1,1 0,1,0 0,-0.25,0.25 -3 0.5 2)
add_test(bn_plane_isect_lseg3_lseg3_case-2_1   tester_bn_plane_isect 2 0,0,0 0,1,1 1,1,1 0,1,1 -2 0 0)
add_test(bn_plane_isect_lseg3_lseg3_case-1_1   tester_bn_plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -1,-1,-1 -1 1.666667 1.333333)
add_test(bn_plane_isect_lseg3_lseg3_case0_1    tester_bn_plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -2,-2,-2 0 1.666667 1)
add_test(bn_plane_isect_lseg3_lseg3_case0_2    tester_bn_plane_isect 2 -5,-5,-5 3,3,3 0,0,0 -3,-3,-3 0 1.666667 0.666667)
add_test(bn_plane_isect_lseg3_lseg3_case0_3    tester_bn_plane_isect 2 -5,-5,-5 5,5,5 -4,-4,-4 2,2,2 0 0.2 0.6)
add_test(bn_plane_isect_lseg3_lseg3_case1_1    tester_bn_plane_isect 2 0,0,0 0,1,1 0,1,0 0,-1,1 1 0.5 0.5)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
