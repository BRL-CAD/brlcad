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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
