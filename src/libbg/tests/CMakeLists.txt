#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2010-2016 United States Government as represented by
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

BRLCAD_ADDEXEC(bg_obr obr.c "libbg;libbn;libbu" NO_INSTALL)

#add_test(NAME bg_obr_simple			COMMAND bg_obr) # Default points


#  ************ polygon.c tests ***********
BRLCAD_ADDEXEC(bg_polygon_triangulate polygon_triangulate.c "libbg;libbn;libbu" NO_INSTALL)

add_test(NAME bg_polygon_triangulate	    COMMAND bg_polygon_triangulate)


#  ************ chull.c tests ***********
# 2D Convex Hull tests
BRLCAD_ADDEXEC(bg_chull chull.c "libbg;libbn;libbu" NO_INSTALL)
CMAKEFILES(chull_test003_bot.asc)
CMAKEFILES(chull_test004_bot.asc)

#  ************ chull3d.cpp tests ***********

# 3D Convex Hull tests
BRLCAD_ADDEXEC(bg_chull3d chull3d.c "libbg;libbn;libbu" NO_INSTALL)



#  ************ tri_tri.c tests ***********

# For COMMAND bg_tri_tri_isect_coplanar, the input format is as follows:
#
# COMMAND bg_tri_tri_isect_coplanar V0 V1 V2 U0 U1 U2 <area_flag> <expected result>
#
# where P and Q are the two triangles in question.  An individual point
# is three integer or floating point numbers separated by commas. The area
# flag tells the function whether or not to require non-zero area for an
# overlap in coplanar cases.

BRLCAD_ADDEXEC(bg_tri_tri_isect_coplanar tri_tri_isect_coplanar.c "libbg;libbn;libbu" NO_INSTALL)

# TODO - need some tests with floating point vertices that are down around the EPSILON threshold - that's
# where the NEAR_ZERO components of the bg_tri_tri_isect_coplanar logic become important.

add_test(NAME bg_tri_tri_isect_coplanar_null_noarea   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  0  0)
add_test(NAME bg_tri_tri_isect_coplanar_vertex_noarea COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        0  1)
add_test(NAME bg_tri_tri_isect_coplanar_edge_noarea   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        0  1)
add_test(NAME bg_tri_tri_isect_coplanar_full_noarea   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    0  1)
add_test(NAME bg_tri_tri_isect_coplanar_null_area   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  1  0)
add_test(NAME bg_tri_tri_isect_coplanar_vertex_area COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        1  0)
add_test(NAME bg_tri_tri_isect_coplanar_edge_area   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        1  0)
add_test(NAME bg_tri_tri_isect_coplanar_full_area   COMMAND bg_tri_tri_isect_coplanar  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    1  1)

# For COMMAND bg_tri_tri_isect, the input format is as follows:
#
# COMMAND bg_tri_tri_isect V0 V1 V2 U0 U1 U2 <expected result>
#
# where P and Q are the two triangles in question.  An individual point
# is three integer or floating point numbers separated by commas.

BRLCAD_ADDEXEC(bg_tri_tri_isect tri_tri_isect.c "libbg;libbn;libbu" NO_INSTALL)

# Test coplanar triangles
add_test(NAME bg_tri_tri_isect_cp_null   COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1.00001,1,0  0)
add_test(NAME bg_tri_tri_isect_cp_vertex COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1.00001,0,0 2,0,0 1,1,0        1)
add_test(NAME bg_tri_tri_isect_cp_edge   COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 1,1,0        1)
add_test(NAME bg_tri_tri_isect_cp_full   COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,0       2,0,0 0.7,0.8,0    1)

# Test more general triangles
add_test(NAME bg_tri_tri_isect_null   COMMAND bg_tri_tri_isect  0,0,0 1,0,0 1,1,0  1,0,1 1,0,1 1,1,1  0)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 textwidth=0 wrapmargin=0
