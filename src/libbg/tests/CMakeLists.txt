#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2010-2021 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#  ************ obr.c tests *************

BRLCAD_ADDEXEC(bg_obr obr.c "libbg;libbn;libbu" TEST)

#BRLCAD_ADD_TEST(NAME bg_obr_simple			COMMAND bg_obr) # Default points


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



CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
