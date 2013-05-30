BRLCAD_ADDEXEC(tester_bn_tri_tri_isect_coplanar bn_tri_tri_isect_coplanar.c "libbu;libbn" NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bn_tri_tri_isect bn_tri_tri_isect.c "libbu;libbn" NO_INSTALL LOCAL)
BRLCAD_ADDEXEC(tester_bn_list bn_list.c "libbu;libbn" NO_INSTALL LOCAL)

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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
