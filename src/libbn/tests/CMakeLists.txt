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
BRLCAD_ADDEXEC(tester_bn_complex bn_complex.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_mat bn_mat.c "libbu;libbn" NO_INSTALL)
BRLCAD_ADDEXEC(tester_bn_tabdata bn_tabdata.c "libbu;libbn" NO_INSTALL)
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

add_test(bn_plane_mk_plane_3pts_fail_same        tester_bn_plane_pt 5 0,0,0 0,0,0 0,0,0 -1)
add_test(bn_plane_mk_plane_3pts_fail_collinear_1 tester_bn_plane_pt 5 0,0,0 5,8,2 0,0,0 -1)
add_test(bn_plane_mk_plane_3pts_fail_collinear_2 tester_bn_plane_pt 5 5,8,2 0,0,0 0,0,0 -1)
add_test(bn_plane_mk_plane_3pts_fail_collinear_3 tester_bn_plane_pt 5 -10,-10,-10 0,0,0 5,5,5 -1)
add_test(bn_plane_mk_plane_3pts_succeed          tester_bn_plane_pt 5 -10,-10,-10 7,3,8 19,4,2 0)


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

#
#  ************ complex.c tests *************

# For tester_bn_complex, the input format is as follows:
#
# tester_bn_complex <function number> <args>

# For function #1 (bn_cx_div), the <args> format is as follows:
#
# AP BP expected_return
#
# where AP and BP are the numbers in question. An individual complex
# number is two integer or floating point numbers separated by commas.

add_test(bn_cx_div_same_1        tester_bn_complex 1  1,3 1,3 1,0)
add_test(bn_cx_div_same_2        tester_bn_complex 1  3,1 3,1 1,0)
add_test(bn_cx_div_dif_1         tester_bn_complex 1  6.3,4.2 9.8,7.7 0.605678,-0.0473186)
add_test(bn_cx_div_dif_2         tester_bn_complex 1  0,0 1,1 0,0)
add_test(bn_cx_div_by_0_1        tester_bn_complex 1  1,1 0,0 1.0e20,1.0e20)

# For function #2 (bn_cx_mul/bn_cx_mul2), the <args> format is as follows:
#
# AP BP expected_return
#
# where AP and BP are the numbers in question. An individual complex
# number is two integer or floating point numbers separated by commas.

add_test(bn_cx_mul_0_1           tester_bn_complex 2 5.2,3.8 0,0 0,0)
add_test(bn_cx_mul_0_2           tester_bn_complex 2 0,0 5.2,3.8 0,0)
add_test(bn_cx_mul_inv_1         tester_bn_complex 2 7.4,2.3 0.123231,-0.0383014 1,0)
add_test(bn_cx_mul_inv_2         tester_bn_complex 2 0.123231,-0.0383014 7.4,2.3 1,0)

# For function #3 (bn_cx_sub), the <args> format is as follows:
#
# AP BP expected_return
#
# where AP and BP are the numbers in question. An individual complex
# number is two integer or floating point numbers separated by commas.

add_test(bn_cx_sub_0_1           tester_bn_complex 3 5.2,3.8 0,0 5.2,3.8)
add_test(bn_cx_sub_0_2           tester_bn_complex 3 0,0 5.2,3.8 -5.2,-3.8)
add_test(bn_cx_sub_same_1        tester_bn_complex 3 1.4,2.9 1.4,2.9 0,0)
add_test(bn_cx_sub_dif_1         tester_bn_complex 3 7.4,-2.3 8.9,6.4 -1.5,-8.7)
add_test(bn_cx_sub_dif_2         tester_bn_complex 3 8.9,6.4 7.4,-2.3 1.5,8.7)

# For function #4 (bn_cx_add), the <args> format is as follows:
#
# AP BP expected_return
#
# where AP and BP are the numbers in question. An individual complex
# number is two integer or floating point numbers separated by commas.

add_test(bn_cx_add_0_1           tester_bn_complex 4 5.2,3.8 0,0 5.2,3.8)
add_test(bn_cx_add_0_2           tester_bn_complex 4 0,0 5.2,3.8 5.2,3.8)
add_test(bn_cx_add_inv_1         tester_bn_complex 4 1.4,2.9 -1.4,-2.9 0,0)
add_test(bn_cx_add_dif_1         tester_bn_complex 4 7.4,-2.3 8.9,6.4 16.3,4.1)
add_test(bn_cx_add_dif_2         tester_bn_complex 4 8.9,6.4 7.4,-2.3 16.3,4.1)

# For function #5 (bn_cx_sqrt), the <args> format is as follows:
#
# IP expected_return
#
# where IP is the number in question. An individual complex number is
# two integer or floating point numbers separated by commas.

add_test(bn_cx_sqrt_1            tester_bn_complex 5  0,2 1,1)
add_test(bn_cx_sqrt_2            tester_bn_complex 5  2,0 1.414214,0)
add_test(bn_cx_sqrt_3            tester_bn_complex 5  0,0 0,0)
add_test(bn_cx_sqrt_4            tester_bn_complex 5 6.3,4.2 2.63360,0.797389)
add_test(bn_cx_sqrt_5            tester_bn_complex 5 9.8,7.7 3.33640,1.15394)

# For function #6 (bn_cx_neg), the <args> format is as follows:
#
# IP expected_return
#
# where IP is the number in question. An individual complex number is
# two integer or floating point numbers separated by commas.

add_test(bn_cx_neg_0_1           tester_bn_complex 6  0,0 0,0)
add_test(bn_cx_neg_0_2           tester_bn_complex 6  2,0 -2,0)
add_test(bn_cx_neg_0_3           tester_bn_complex 6  0,2 0,-2)
add_test(bn_cx_neg_dif_1         tester_bn_complex 6 6.3,4.2 -6.3,-4.2)

# For function #7 (bn_cx_conj), the <args> format is as follows:
#
# IP expected_return
#
# where IP is the number in question. An individual complex number is
# two integer or floating point numbers separated by commas.

add_test(bn_cx_conj_0_1          tester_bn_complex 7  0,0 0,0)
add_test(bn_cx_conj_0_2          tester_bn_complex 7  2,0 2,0)
add_test(bn_cx_conj_0_3          tester_bn_complex 7  0,2 0,-2)
add_test(bn_cx_conj_dif_1        tester_bn_complex 7 6.3,4.2 6.3,-4.2)

# For function #8 (bn_cx_real/bn_cx_imag), the <args> format is as follows:
#
# IP expected_re,expected_im
#
# where IP is the number in question. An individual complex number is
# two integer or floating point numbers separated by commas.

add_test(bn_cx_parts_1           tester_bn_complex 8  0,0 0,0)
add_test(bn_cx_parts_2           tester_bn_complex 8  1,2 1,2)
add_test(bn_cx_parts_3           tester_bn_complex 8  2,1 2,1)


#
#  *************** mat.c tests ***************
#
# For tester_bn_mat, input is as follows:
# tester_bn_mat <function_number> <args>
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

add_test(bn_mat_mul_0_1          tester_bn_mat 1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1.0,2.1,3.3,4.4,5.5,6.6,7.7,8.8,9.9,10.1,11.11,12.12,13.13,14.14,15.15,16.16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
add_test(bn_mat_mul_0_2          tester_bn_mat 1 2.8,1.7,3205.555,87.76,55.0,66.0,77.0,0.0,0.0,10.1,46.8,537.999,13.0,14.0,15.0,24382.5373 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
add_test(bn_mat_mul_id_1         tester_bn_mat 1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 33.34,28.7135672,44.84,55.85,1,1,0,0,0,7843.4444,11,12,473.232,83.17,75.0,8.417 33.34,28.7135672,44.84,55.85,1,1,0,0,0,7843.4444,11,12,473.232,83.17,75.0,8.417)
add_test(bn_mat_mul_id_2         tester_bn_mat 1 3688.701794246889,5712.945961512324,8172.282142910184,5109.8260742578495,2697.196099611537,2586.742536023153,5067.2882336104285,1149.9923643570387,6827.968448547921,9548.849169087358,5418.682604729545,9117.6798488191,5478.474626204755,7525.19019682893,3172.945417883698,8609.907811093646 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 3688.701794246889,5712.945961512324,8172.282142910184,5109.8260742578495,2697.196099611537,2586.742536023153,5067.2882336104285,1149.9923643570387,6827.968448547921,9548.849169087358,5418.682604729545,9117.6798488191,5478.474626204755,7525.19019682893,3172.945417883698,8609.907811093646)
add_test(bn_mat_mul_id_id        tester_bn_mat 1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
add_test(bn_mat_mul_dif_1        tester_bn_mat 1 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1 80,70,60,50,240,214,188,162,400,358,316,274,560,502,444,386)
add_test(bn_mat_mul_dif_2        tester_bn_mat 1 3.2,62.9,53.0,71.5,98.6,75.4,9.6,89.9,37.7,49.0,60.2,55.2,27.8,40.2,0.5,78.1 37.1,73.1,4.9,72.1,68.2,64.7,75.7,87.4,85.8,45.0,94.5,30.8,95.9,45.9,45.4,49.1 15812.75,9970.4,13031.81,10871.23,18245.43,16644.45,11179.58,18408.79,15199.31,11168.85,12089.01,11565.25,11305.71,8240.41,6772.35,9367.97)

# For function #2 (bn_mat_mul3), the <args> format is as follows:
#
# M1 M2 M3 <expected_result>
#
# where M1 M2 M3 are the three matrices to be multiplied.
# A matrix is 16 floating point numbers separated by commas.

add_test(bn_mat_mul3_0_1         tester_bn_mat 2 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
add_test(bn_mat_mul3_0_id        tester_bn_mat 2 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
add_test(bn_mat_mul3_id_1        tester_bn_mat 2 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1 80,70,60,50,240,214,188,162,400,358,316,274,560,502,444,386)
add_test(bn_mat_mul3_dif         tester_bn_mat 2 3.1,4.2,1.9,1.8,2.9,0.3,4.1,4.5,2.9,2.7,0.7,1.7,1.6,0.8,4.5,5.0 1.1,2.1,2.3,3.5,2.7,1.8,2.6,3.8,1.5,0.7,3.0,4.4,2.3,0.3,4.5,4.3 3.2,2.0,2.6,0.0,2.1,4.0,0.5,1.6,4.3,3.1,2.7,4.7,0.9,3.3,2.4,4.7 278.616,347.578,253.473,376.876,304.197,369.044,283.557,434.156,203.015,252.984,183.327,274.048,315.303,376.398,294.975,446.11)

# For function #3 (bn_mat_mul4), the <args> format is as follows:
#
# M1 M2 M3 M4 <expected_result>
#
# where M1 M2 M3 M4 are the four matrices to be multiplied.
# A matrix is 16 floating point numbers separated by commas.

add_test(bn_mat_mul4_id_4        tester_bn_mat 3 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
add_test(bn_mat_mul4_0_4         tester_bn_mat 3 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
add_test(bn_mat_mul4_id_0        tester_bn_mat 3 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
add_test(bn_mat_mul4_same        tester_bn_mat 3 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 113960,129040,144120,159200,263272,298128,332984,367840,412584,467216,521848,576480,561896,636304,710712,785120)

# For function #4 (bn_matXvec), the <args> format is as follows:
#
# M V <expected_result>
#
# where M is the matrix
#       V is the (column) vector to which the matrix will be multiplied, MxV.
# A matrix is 16 floating point numbers separated by commas.
# A vector is 4 floating point numbers separated by commas.

add_test(bn_matXvec_id           tester_bn_mat 4 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 2.78,3.45,16.7,38.22 2.78,3.45,16.7,38.22)
add_test(bn_matXvec_0_1          tester_bn_mat 4 18.59,25.82,46.39,16.56,46.13,22.16,29.66,92.89,67.58,79.96,59.47,51.4,55.83,48.5,82.51,73.52 0,0,0,0 0,0,0,0)
add_test(bn_matXvec_0_2          tester_bn_mat 4 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 1.1,2.2,3.3,4.4 0,0,0,0)
add_test(bn_matXvec_dif_1        tester_bn_mat 4 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4 30,70,110,150)
add_test(bn_matXvec_dif_2        tester_bn_mat 4 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 0,1,2,3 14,38,62,86)
add_test(bn_matXvec_dif_3        tester_bn_mat 4 18.59,25.82,46.39,16.56,46.13,22.16,29.66,92.89,67.58,79.96,59.47,51.4,55.83,48.5,82.51,73.52 7.31,68.38,65.97,3.26  5015.7984,4112.0027,10052.4745,9407.4072)

# For function #5 (bn_mat_inverse), the <args> format is as follows:
#
# 0|1 M [expected_result]
#
# where 0 or 1 indicates whether the matrix is singular
#       M is the matrix to be inverted.
# A matrix is 16 floating point numbers separated by commas.

add_test(bn_mat_inverse_0        tester_bn_mat 5 1 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
add_test(bn_mat_inverse_id       tester_bn_mat 5 0 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
add_test(bn_mat_inverse_sing_1   tester_bn_mat 5 1 1,2,2,0,1,1,3,4,2,2,1,0,0,0,0,0)
add_test(bn_mat_inverse_sing_2   tester_bn_mat 5 1 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)
add_test(bn_mat_inverse_1        tester_bn_mat 5 0 14.16,71.27,95.53,77.27,91.51,26.83,17.56,6.94,30.11,40.47,38.1,39.27,56.52,90.12,87.96,84.3 0.00836179,0.0114599,0.0844829,-0.0479631,-0.0600379,-0.0107874,-0.349984,0.218954,0.0425546,0.0207413,-0.0527631,-0.0161344,0.0141745,-0.0177931,0.372557,-0.173216)

# For function #6 (bn_mat_trn), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to transpose.
# A matrix is 16 floating point numbers separated by commas.

add_test(bn_mat_trn_id           tester_bn_mat 6 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
add_test(bn_mat_trn_same         tester_bn_mat 6 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
add_test(bn_mat_trn_1            tester_bn_mat 6 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,5,9,13,2,6,10,14,3,7,11,15,4,8,12,16)
add_test(bn_mat_trn_2            tester_bn_mat 6 99.01,41.88,17.07,37.47,38.8,42.89,34.48,82.84,59.97,23.74,6.98,27.81,1.64,86.35,43.57,13.87 99.01,38.8,59.97,1.64,41.88,42.89,23.74,86.35,17.07,34.48,6.98,43.57,37.47,82.84,27.81,13.87)

# For function #7 (bn_mat_is_identity), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to test.
# A matrix is 16 floating point numbers separated by commas.

add_test(bn_mat_is_identity_id   tester_bn_mat 7 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
add_test(bn_mat_is_identity_0    tester_bn_mat 7 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
add_test(bn_mat_is_identity_1    tester_bn_mat 7 15.58,76.15,18.45,80.55,52.36,28.49,85.75,56.75,76.44,66.57,32.39,52.18,39.21,96.09,98.66,30.95 0)
add_test(bn_mat_is_identity_2    tester_bn_mat 7 1.001,0,0,0,0,1.001,0,0,0,0,1.001,0,0,0,0,1.001 0)

# For function #8 (bn_mat_det3), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to find the upper-left 3x3 determinant of.
# A matrix is 16 floating point numbers separated by commas.

add_test(bn_mat_det3_id          tester_bn_mat 8 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
add_test(bn_mat_det3_0           tester_bn_mat 8 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
add_test(bn_mat_det3_1           tester_bn_mat 8 77.64,50.22,46.68,16.35,41.25,71.79,24.01,23.88,37.37,94.83,34.25,4.43,95.01,81.8,1.45,45.57 45601.558488)
add_test(bn_mat_det3_2           tester_bn_mat 8 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0)

# For function #9 (bn_mat_determinant), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to find the determinant of.
# A matrix is 16 floating point numbers separated by commas.

add_test(bn_mat_determinant_id   tester_bn_mat 9 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 1)
add_test(bn_mat_determinant_0    tester_bn_mat 9 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
add_test(bn_mat_determinant_1    tester_bn_mat 9 80.22,28.45,95.46,74.05,53.38,74.98,71.55,24.86,65.01,46.42,64.24,49.13,75.74,79.22,64.61,17.33 2.1004520202541295e6)
add_test(bn_mat_determinant_2    tester_bn_mat 9 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0)

# For function #10 (bn_mat_is_equal), the <args> format is as follows:
#
# M1 M2 <expected_result>
#
# where M1 and M2 are the two matrices to be compared.
# A matrix is 16 floating point numbers separated by commas.

add_test(bn_mat_is_equal_1       tester_bn_mat 10 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 1)
add_test(bn_mat_is_equal_2       tester_bn_mat 10 70.46,38.7,1.57,84.19,38.53,52.52,89.28,8.58,89.96,61.36,5.45,63.62,99.98,82.15,65.44,67.46 70.46,38.7,1.57,84.19,38.53,52.52,89.28,8.58,89.96,61.36,5.45,63.62,99.98,82.15,65.44,67.46 1)
add_test(bn_mat_is_equal_3       tester_bn_mat 10 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 0)
add_test(bn_mat_is_equal_4       tester_bn_mat 10 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 71.53,18.85,47.48,24.56,42.34,98.18,42.1,74.61,30.47,49.18,63.06,2.11,58.31,60.61,0.62,34.23 0)

# For function #11 (bn_mat_atan2), the <args> format is as follows:
#
# y x <expected_result>
#
# where x and y are the floating point arguments to the atan2 function

add_test(bn_atan2_0_0            tester_bn_mat 11 0 0 0)
add_test(bn_atan2_0_1            tester_bn_mat 11 2.8 0 1.570796326794896)
add_test(bn_atan2_0_2            tester_bn_mat 11 -2.8 0 -1.570796326794896)
add_test(bn_atan2_1              tester_bn_mat 11 2.55 8.76 0.283268001141651)
add_test(bn_atan2_2              tester_bn_mat 11 -2.8 -7.2 -2.770701364777130)

# For function #12 (bn_vtoh_move), the <args> format is as follows:
#
# V <expected_result>
#
# where V is the 3-vector to move to a homogeneous vector.
# A vector is three floating point numbers separated by commas.

add_test(bn_vtoh_move            tester_bn_mat 12 2.6,-5.5,8.12 2.6,-5.5,8.12,1)

# For function #13 (bn_vtoh_move), the <args> format is as follows:
#
# H <expected_result>
#
# where H is the homogeneous vector to move to a 3-vector.
# A homogeneous vector is four floating point numbers separated by commas.

add_test(bn_htov_move            tester_bn_mat 13 1.1,2.2,3.3,7.4 0.14864864864864866,0.2972972972972973,0.4459459459459459)

# For function #14 (bn_mat_ae), the <args> format is as follows:
#
# az el <expected_result>
#
# where az is the azimuth and el is the elevation to generate a
# transform matrix.
# az and el are floating point numbers and are in degrees.

add_test(bn_mat_ae               tester_bn_mat 14 0 90 0,0,-1,0,0,1,0,0,1,0,0,0,0,0,0,1)

# For function #15 (bn_ae_vec), the <args> format is as follows:
#
# V <expected_az> <expected_el>
#
# where V is the vector to extract az and el info from.
#
# Functions 15-17 were verified using the website below.
# http://www.random-science-tools.com/maths/coordinate-converter.htm

add_test(bn_ae_vec_0             tester_bn_mat 15 0,1,0 90 0)
add_test(bn_ae_vec_1             tester_bn_mat 15 5.7,-2.34,19.61 337.68055960702 72.556852698595)
add_test(bn_ae_vec_2             tester_bn_mat 15 -23.78,42,3.141 119.518125952119 3.7234738236524)

# For function #16 (bn_vec_ae), the <args> format is as follows:
#
# az el <expected_result>
#
# where az is the azimuth and el is the elevation to generate a vector
# az and el are floting point numbers and are in radians.

# These two tests fail for some reason. It could be a bug in bn_vec_ae().
# The value of the returned vector seems to always have the same z value
# irrespective of the input.

add_test(bn_vec_ae_1             tester_bn_mat 16 1 1 0.291926581820,0.454648713558,0.841470984697)
add_test(bn_vec_ae_2             tester_bn_mat 16 0.5 1.2 0.317998846662,0.173723561699,0.932039085893)

# For function #17 (bn_vec_aed), the <args> format is as follows:
#
# az el d <expected_result>
#
# where az is the azimuth, el is the elevation and d is the distance
# to generate a vector
# az and el are floting point numbers and are in radians.
# d is a floating point number.

add_test(bn_vec_aed_1            tester_bn_mat 17 1 1 5 1.45963290910,2.27324356779,4.20735492349)
add_test(bn_vec_aed_2            tester_bn_mat 17 0.3845 0.286 18.3354 16.3062567701,6.59816438267,5.17272752864)

# For function #18 (bn_mat_angles), the <args> format is as follows:
#
# x y z <expected_result>
#
# where x, y, z are the angles to rotate about their respective axes
# x, y, z are floating point numbers and are in degrees.
#
# Functions 18 and 19 were verified using the website below
# http://toolserver.org/~dschwen/tools/rotationmatrix.html

add_test(bn_mat_angles_1         tester_bn_mat 18 30 40 50 0.4924038765,-0.5868240888,0.6427876097,0,0.8700019038,0.310468461,-0.3830222216,0,0.0252013863,0.7478280708,0.6634139482,0,0,0,0,1)
add_test(bn_mat_angles_2         tester_bn_mat 18 84.23 19.5 38.9 0.733604282,-0.591944033,0.3338068592,0,0.3215992025,-0.1303153824,-0.9378655842,0,0.5986641049,0.7953742282,0.0947688064,0,0,0,0,1)

# For function #19 (bn_mat_angles_rad), the <args> format is as follows:
#
# x y z <expected_result>
#
# where x, y, z are the angles to rotate about their respective axes
# x, y, z are floating point numbers and are in radians.

add_test(bn_mat_angles_rad_1     tester_bn_mat 19 1.14 0.856 0.321 0.6219827767,-0.2068090198,0.7552267572,0,0.7829298453,0.1797494268,-0.5955761923,0,-0.0125810482,0.9617277021,0.2737180014,0,0,0,0,1)
add_test(bn_mat_angles_rad_2     tester_bn_mat 19 0.6 1 0.92 0.3273260276,-0.4298653899,0.8414709848,0,0.9444818397,0.1219905484,-0.3050776304,0,0.0284908076,0.8946139126,0.4459307359,0,0,0,0,1)

# For function #20 (bn_eigen2x2), the <args> format is as follows:
#
# a b c <expected_val1> <expected_val2> <expected_vec1> <expected_vec2>
#
# where a, b, c are the values of the symmetric 2x2 matrix.
# The eigenvalue with the lowest absolute value will be val1, and its
# eigenvector will be in vec1.
# An eigen vector is two floating point numbers separated by commas.

add_test(bn_eigen2x2_1           tester_bn_mat 20 2.18 13.9 6.6 -9.68459058019096 18.464590580191 0.760598630446286,-0.649222398999938,0 0.649222398999938,0.760598630446286,0)
add_test(bn_eigen2x2_2           tester_bn_mat 20 1 2 3 -0.23606797749979 4.23606797749979 0.85065080835204,-0.525731112119133,0 0.525731112119133,0.85065080835204,0)

# For function #21 (bn_mat_xrot), the <args> format is as follows:
#
# sinx cosx <expected_result>
#
# where sinx and cosx are the sine and cosine of the angle of rotation
# about the x-axis.
# sinx and cosx are floating point values.

add_test(bn_mat_xrot_1           tester_bn_mat 21 0.342020143325668 0.939692620785908 1,0,0,0,0,0.9396926208,-0.3420201433,0,0,0.3420201433,0.9396926208,0,0,0,0,1)
add_test(bn_mat_xrot_2           tester_bn_mat 21 -0.78260815685241 0.622514636637619 1,0,0,0,0,0.6225146366,0.7826081569,0,0,-0.7826081569,0.6225146366,0,0,0,0,1)

# For function #22 (bn_mat_yrot), the <args> format is as follows:
#
# siny cosy <expected_result>
#
# where siny and cosy are the sine and cosine of the angle of rotation
# about the y-axis.
# siny and cosy are floating point values.

add_test(bn_mat_yrot_1           tester_bn_mat 22 0.30901699437494 0.951056516295153 0.9510565163,0,-0.3090169944,0,0,1,0,0,0.3090169944,0,0.9510565163,0,0,0,0,1)
add_test(bn_mat_yrot_2           tester_bn_mat 22 0.17364817766693 0.984807753012208 0.9848077530122,0,-0.1736481777,0,0,1,0,0,0.1736481777,0,0.984807753,0,0,0,0,1)

# For function #23 (bn_mat_zrot), the <args> format is as follows:
#
# sinz cosz <expected_result>
#
# where sinz and cosz are the sine and cosine of the angle of rotation
# about the z-axis.
# sinz and cosz are floating point values.

add_test(bn_mat_zrot_1           tester_bn_mat 23 0.669130606358858 0.743144825477394 0.743144825477394,-0.669130606358858,0,0,0.669130606358858,0.743144825477394,0,0,0,0,1,0,0,0,0,1)
add_test(bn_mat_zrot_2           tester_bn_mat 23 -0.99619469809174 -0.087155742747658 -0.087155742747658,0.996194698091745,0,0,-0.996194698091746,-0.087155742747658,0,0,0,0,1,0,0,0,0,1)

# For function #24 (bn_mat_scale_about_pt), the <args> format is as follows:
#
# P s <expected_result>
#
# where P is the point to scale about and s is the uniform scale to
# be applied.
# A point is three floating point numbers separated by commas.
# Scale is a floating point number.

add_test(bn_mat_scale_about_pt_1 tester_bn_mat 24 3,4,5 1.5 0 1,0,0,-1,0,1,0,-1.333333333333333,0,0,1,-1.666666666666667,0,0,0,0.666666666666667)
add_test(bn_mat_scale_about_pt_2 tester_bn_mat 24 2.18,-4.55,-17.4 0.31 0 1,0,0,4.852258064516120,0,1,0,-10.127419354838700,0,0,1,-38.729032258064500,0,0,0,3.225806451612900)

# For function #25 (bn_mat_xform_about_pt), the <args> format is as follows:
#
# M P <expected_result>
#
# where M is the transform matrix, P is the point to apply from.
# A matrix is 16 floating point values separated by commas.
# A point is three floating point values separated by commas.

add_test(bn_mat_xform_about_pt_1 tester_bn_mat 25 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 -2,4.5,6.7 -25,-26,-27,219.9,63.5,69,74.5,-602.65,96.1,103.8,111.5,-902.75,13,14,15,-121.5)

# For function #26 (bn_mat_ck), the <args> format is as follows:
#
# M <expected_result>
#
# where M is the matrix to check for axis perpendicularity
# A matrix is 16 floating point values separated by commas.

add_test(bn_mat_ck_1             tester_bn_mat 26 -0.064769339233561,-0.740316935054896,-0.669130606358858,0,-0.920939044198179,-0.213876288688268,0.325773249374894,0,-0.384286624235859,0.637328619165857,-0.667934144677115,0,0,0,0,1 0)
add_test(bn_mat_ck_2             tester_bn_mat 26 -0.384795044679446,-0.906520316365329,0.17364817766693,23.835,-0.772197474581702,0.419235049422787,0.477444272753497,-1.23,-0.505612335529687,0.049627506006009,-0.861332268528144,-46.321,0,0,0,1 0)
add_test(bn_mat_ck_3             tester_bn_mat 26 -0.38457345,-0.944,0.46483,23.835,0.27474,0.4192787,0.4272753497,-1.23,-0.87,0.06009,0.999144,-46.321,0,0,0,1 -1)

# For function #27 (bn_mat_dup), there is one argument to keep main()
# happy, which is ignored.
# A matrix is initiated, copied and the dereferenced pointer compared in the funcion.

add_test(bn_mat_dup              tester_bn_mat 27 0)


#
#  *************** tabdata.c tests ***************
#
# For tester_bn_tabdata, input is as follows:
# tester_bn_tabdata <function_number> <args>

# For function #1 (bn_table_make_uniform), the <args> format is as follows:
#
# first last num expected
#
# where first last and num will be used to make a uniform table, and
# expected is the expected result table.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx).

add_test(bn_table_make_uniform tester_bn_tabdata 1 0 100 10 10,0,10,20,30,40,50,60,70,80,90,100)

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

add_test(bn_tabdata_add tester_bn_tabdata 2 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 9,5,9,6,14)

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

add_test(bn_tabdata_mul tester_bn_tabdata 3 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45)

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

add_test(bn_tabdata_mul3 tester_bn_tabdata 4 5,0,20,40,60,80,100 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45 64,36,324,64,2025)


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

add_test(bn_tabdata_incr_mul3_scale tester_bn_tabdata 5 5,0,20,40,60,80,100 5,4,3,2,1 1,2,3,4,5 8,3,6,2,9 8,6,18,8,45 2 133,76,651,130,4051)


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

add_test(bn_tabdata_incr_mul2_scale tester_bn_tabdata 6 5,0,20,40,60,80,100 5,4,3,2,1 1,2,3,4,5 8,3,6,2,9 2 21,16,39,18,91)


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

add_test(bn_tabdata_scale tester_bn_tabdata 7 5,0,20,40,60,80,100 1,2,3,4,5 2 2,4,6,8,10)


# For function #8 (bn_table_scale), the <args> format is as follows:
#
# table scale expected_table
#
# where table will be scaled by scale.  A table is a list of
# comma-separated numbers, where the first one is the number of
# entries: nx,x0,x1,...,x(nx).

add_test(bn_table_scale tester_bn_tabdata 8 5,0,20,40,60,80,100 2 5,0,40,80,120,160,200)


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

add_test(bn_tabdata_join1 tester_bn_tabdata 9 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2 11,10,9,8,7)


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

add_test(bn_tabdata_join2 tester_bn_tabdata 10 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2,4,6,8,10 2 3 17,22,27,32,37)


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

add_test(bn_tabdata_blend2 tester_bn_tabdata 11 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 2 0.5 4.5,6,7.5,9,10.5)


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

add_test(bn_tabdata_blend3 tester_bn_tabdata 12 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 3,2,4,1,5 2 0.5 3 13.5,12,19.5,12,25.5)


# For function #13 (bn_tabdata_area1), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of the tabdata (which is a tabdata over table), will
# be calculated. The area will be calculated using intepretation #1
# (see bn.h). A table is a list of comma-separated numbers, where the
# first one is the number of entries: nx,x0,x1,...,x(nx); a tabdata is
# a list of comma-separated numbers with the same length as the table
# to which it refers; if the table entry is nx,x0,x1,...,x(nx), then
# the tabdata entry is y0,y1,...y(nx-1)

add_test(bn_tabdata_area1 tester_bn_tabdata 13 5,0,20,40,60,80,100 1,2,3,4,5 15)


# For function #14 (bn_tabdata_area2), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of the tabdata (which is a tabdata over table), will
# be calculated. The area will be calculated using intepretation #2
# (see bn.h). A table is a list of comma-separated numbers, where the
# first one is the number of entries: nx,x0,x1,...,x(nx); a tabdata is
# a list of comma-separated numbers with the same length as the table
# to which it refers; if the table entry is nx,x0,x1,...,x(nx), then
# the tabdata entry is y0,y1,...y(nx-1)

add_test(bn_tabdata_area2 tester_bn_tabdata 14 5,0,20,40,60,80,100 1,2,3,4,5 300)



# For function #15 (bn_tabdata_mul_area1), the <args> format is as follows:
#
# table tabdata1 tabdata2 expected_result
#
# where the area of tabdata1*tabdata2 (which are tabdatas over table),
# will be calculated. The area will be calculated using intepretation
# #1 (see bn.h). A table is a list of comma-separated numbers, where
# the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

add_test(bn_tabdata_mul_area1 tester_bn_tabdata 15 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 35)


# For function #16 (bn_tabdata_mul_area2), the <args> format is as follows:
#
# table tabdata expected_result
#
# where the area of tabdata1*tabdata2 (which are tabdatas over table),
# will be calculated. The area will be calculated using intepretation
# #2 (see bn.h). A table is a list of comma-separated numbers, where
# the first one is the number of entries: nx,x0,x1,...,x(nx); a
# tabdata is a list of comma-separated numbers with the same length as
# the table to which it refers; if the table entry is
# nx,x0,x1,...,x(nx), then the tabdata entry is y0,y1,...y(nx-1)

add_test(bn_tabdata_mul_area2 tester_bn_tabdata 16 5,0,20,40,60,80,100 1,2,3,4,5 5,4,3,2,1 700)


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

add_test(bn_table_lin_interp_1 tester_bn_tabdata 17 5,0,20,40,60,80,100 1,2,3,4,5 30 2.5)
add_test(bn_table_lin_interp_2 tester_bn_tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 30 3)
add_test(bn_table_lin_interp_3 tester_bn_tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 45 3.5)
add_test(bn_table_lin_interp_4 tester_bn_tabdata 17 5,0,20,30,60,80,100 1,2,3,4,5 25 2.5)
add_test(bn_table_lin_interp_5 tester_bn_tabdata 17 5,0,20,40,60,80,100 1,2,3,4,5 800 0)


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

add_test(bn_tabdata_copy tester_bn_tabdata 18 5,0,20,40,60,80,100 1,2,3,4,5)


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

add_test(bn_tabdata_dup tester_bn_tabdata 19 5,0,20,40,60,80,100 1,2,3,4,5)


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

add_test(bn_tabdata_get_constval tester_bn_tabdata 20 5,0,20,40,60,80,100 3 3,3,3,3,3)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
