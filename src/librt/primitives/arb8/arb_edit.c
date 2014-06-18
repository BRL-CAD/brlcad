/*                      A R B _ E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup arb_edit */
/** @{ */
/** @file primitives/arb8/arb_edit.c
 *
 * Editing operations on arb primitives.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rt/arb_edit.h"
#include "raytrace.h"


void
ext4to6(int pt1, int pt2, int pt3, struct rt_arb_internal *arb, fastf_t peqn[7][4])
{
    point_t pts[8];
    int i;

    VMOVE(pts[0], arb->pt[pt1]);
    VMOVE(pts[1], arb->pt[pt2]);
    VMOVE(pts[4], arb->pt[pt3]);
    VMOVE(pts[5], arb->pt[pt3]);

    /* extrude "distance" to get remaining points */
    VADD2(pts[2], pts[1], &peqn[6][0]);
    VADD2(pts[3], pts[0], &peqn[6][0]);
    VADD2(pts[6], pts[4], &peqn[6][0]);
    VMOVE(pts[7], pts[6]);

    /* copy to the original record */
    for (i=0; i<8; i++)
	VMOVE(arb->pt[i], pts[i]);
}

int
mv_edge(struct rt_arb_internal *arb,
	const vect_t thru,
	const int bp1, const int bp2,
	const int end1, const int end2,
	const vect_t dir,
	const struct bn_tol *tol,
       	fastf_t peqn[7][4])
{
    fastf_t t1, t2;

    if (bn_isect_line3_plane(&t1, thru, dir, peqn[bp1], tol) < 0 ||
	bn_isect_line3_plane(&t2, thru, dir, peqn[bp2], tol) < 0) {
	return 1;
    }

    VJOIN1(arb->pt[end1], thru, t1, dir);
    VJOIN1(arb->pt[end2], thru, t2, dir);

    return 0;
}

int
arb_extrude(struct rt_arb_internal *arb,
	int face, fastf_t dist,
	const struct bn_tol *tol,
	fastf_t peqn[7][4])
{
    int type, prod, i, j;
    int uvec[8], svec[11];
    int pt[4];
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
    struct rt_arb_internal larb;	/* local copy of arb for new way */
    RT_ARB_CK_MAGIC(arb);

    if (rt_arb_get_cgtype(&type, arb, tol, uvec, svec) == 0) type = 0;
    if (type != 8 && type != 6 && type != 4) {return 1;}

    memcpy((char *)&larb, (char *)arb, sizeof(struct rt_arb_internal));

    if ((type == ARB6 || type == ARB4) && face < 1000) {
	/* 3 point face */
	pt[0] = face / 100;
	i = face - (pt[0]*100);
	pt[1] = i / 10;
	pt[2] = i - (pt[1]*10);
	pt[3] = 1;
    } else {
	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);
    }

    /* user can input face in any order - will use product of
     * face points to distinguish faces:
     *    product       face
     *       24         1234 for ARB8
     *     1680         5678 for ARB8
     *      252         2367 for ARB8
     *      160         1548 for ARB8
     *      672         4378 for ARB8
     *       60         1256 for ARB8
     *	     10	         125 for ARB6
     *	     72	         346 for ARB6
     * --- special case to make ARB6 from ARB4
     * ---   provides easy way to build ARB6's
     *        6	         123 for ARB4
     *	      8	         124 for ARB4
     *	     12	         134 for ARB4
     *	     24	         234 for ARB4
     */
    prod = 1;
    for (i = 0; i <= 3; i++) {
	prod *= pt[i];
	if (type == ARB6 && pt[i] == 6)
	    pt[i]++;
	if (type == ARB4 && pt[i] == 4)
	    pt[i]++;
	pt[i]--;
	if (pt[i] > 7) {
	    return 1;
	}
    }

    /* find plane containing this face */
    if (bn_mk_plane_3pts(peqn[6], larb.pt[pt[0]], larb.pt[pt[1]],
			 larb.pt[pt[2]], tol)) {
	return 1;
    }

    /* get normal vector of length == dist */
    for (i = 0; i < 3; i++)
	peqn[6][i] *= dist;

    /* protrude the selected face */
    switch (prod) {

	case 24:   /* protrude face 1234 */
	    if (type == ARB6) {
		return 1;
	    }
	    if (type == ARB4)
		goto a4toa6;	/* extrude face 234 of ARB4 to make ARB6 */

	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VADD2(larb.pt[j], larb.pt[i], peqn[6]);
	    }
	    break;

	case 6:		/* extrude ARB4 face 123 to make ARB6 */
	case 8:		/* extrude ARB4 face 124 to make ARB6 */
	case 12:	/* extrude ARB4 face 134 to Make ARB6 */
    a4toa6:
	    ext4to6(pt[0], pt[1], pt[2], &larb, peqn);
	    type = ARB6;
	    /* TODO - solid edit menu was called here in MGED - why? */
	    break;

	case 1680:   /* protrude face 5678 */
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VADD2(larb.pt[i], larb.pt[j], peqn[6]);
	    }
	    break;

	case 60:   /* protrude face 1256 */
	case 10:   /* extrude face 125 of ARB6 */
	    VADD2(larb.pt[3], larb.pt[0], peqn[6]);
	    VADD2(larb.pt[2], larb.pt[1], peqn[6]);
	    VADD2(larb.pt[7], larb.pt[4], peqn[6]);
	    VADD2(larb.pt[6], larb.pt[5], peqn[6]);
	    break;

	case 672:	/* protrude face 4378 */
	case 72:	/* extrude face 346 of ARB6 */
	    VADD2(larb.pt[0], larb.pt[3], peqn[6]);
	    VADD2(larb.pt[1], larb.pt[2], peqn[6]);
	    VADD2(larb.pt[5], larb.pt[6], peqn[6]);
	    VADD2(larb.pt[4], larb.pt[7], peqn[6]);
	    break;

	case 252:   /* protrude face 2367 */
	    VADD2(larb.pt[0], larb.pt[1], peqn[6]);
	    VADD2(larb.pt[3], larb.pt[2], peqn[6]);
	    VADD2(larb.pt[4], larb.pt[5], peqn[6]);
	    VADD2(larb.pt[7], larb.pt[6], peqn[6]);
	    break;

	case 160:   /* protrude face 1548 */
	    VADD2(larb.pt[1], larb.pt[0], peqn[6]);
	    VADD2(larb.pt[5], larb.pt[4], peqn[6]);
	    VADD2(larb.pt[2], larb.pt[3], peqn[6]);
	    VADD2(larb.pt[6], larb.pt[7], peqn[6]);
	    break;

	case 120:
	case 180:
	    return 1;

	default:
	    return 1;
    }

    /* redo the plane equations */
    if (rt_arb_calc_planes(&error_msg, &larb, type, peqn, tol)) {
	bu_vls_free(&error_msg);
	return 1;
    }
    bu_vls_free(&error_msg);

    /* copy local copy back to original */
    memcpy((char *)arb, (char *)&larb, sizeof(struct rt_arb_internal));

    return 0;
}


int
arb_permute(struct rt_arb_internal *arb, const char *encoded_permutation, const struct bn_tol *tol)
{
    struct rt_arb_internal larb;		/* local copy of solid */
    struct rt_arb_internal tarb;		/* temporary copy of solid */
    static size_t min_tuple_size[9] = {0, 0, 0, 0, 3, 2, 2, 1, 3};
    int vertex, i, k;
    size_t arglen;
    size_t face_size;	/* # vertices in THE face */
    int uvec[8], svec[11];
    int type;
    char **p;

    /*
     * The Permutations
     *
     * Each permutation is encoded as an 8-character string,
     * where the ith character specifies which of the current vertices
     * (1 through n for an ARBn) should assume the role of vertex i.
     * Wherever the internal representation of the ARB as an ARB8
     * stores a redundant copy of a vertex, the string contains a '*'.
     */
    static char *perm4[4][7] = {
	{"123*4***", "124*3***", "132*4***", "134*2***", "142*3***",
	    "143*2***", 0},
	{"213*4***", "214*3***", "231*4***", "234*1***", "241*3***",
	    "243*1***", 0},
	{"312*4***", "314*2***", "321*4***", "324*1***", "341*2***",
	    "342*1***", 0},
	{"412*3***", "413*2***", "421*3***", "423*1***", "431*2***",
	    "432*1***", 0}
    };
    static char *perm5[5][3] = {
	{"12345***", "14325***", 0},
	{"21435***", "23415***", 0},
	{"32145***", "34125***", 0},
	{"41235***", "43215***", 0},
	{0, 0, 0}
    };
    static char *perm6[6][3] = {
	{"12345*6*", "15642*3*", 0},
	{"21435*6*", "25631*4*", 0},
	{"34126*5*", "36524*1*", 0},
	{"43216*5*", "46513*2*", 0},
	{"51462*3*", "52361*4*", 0},
	{"63254*1*", "64153*2*", 0}
    };
    static char *perm7[7][2] = {
	{"1234567*", 0},
	{0, 0},
	{0, 0},
	{"4321576*", 0},
	{0, 0},
	{"6237514*", 0},
	{"7326541*", 0}
    };
    static char *perm8[8][7] = {
	{"12345678", "12654378", "14325876", "14852376",
	    "15624873", "15842673", 0},
	{"21436587", "21563487", "23416785", "23761485",
	    "26513784", "26731584", 0},
	{"32147658", "32674158", "34127856", "34872156",
	    "37624851", "37842651", 0},
	{"41238567", "41583267", "43218765", "43781265",
	    "48513762", "48731562", 0},
	{"51268437", "51486237", "56218734", "56781234",
	    "58416732", "58761432", 0},
	{"62157348", "62375148", "65127843", "65872143",
	    "67325841", "67852341", 0},
	{"73268415", "73486215", "76238514", "76583214",
	    "78436512", "78563412", 0},
	{"84157326", "84375126", "85147623", "85674123",
	    "87345621", "87654321", 0}
    };
    static int vert_loc[] = {
	/*		-----------------------------
	 *		   Array locations in which
	 *		   the vertices are stored
	 *		-----------------------------
	 *		1   2   3   4   5   6   7   8
	 *		-----------------------------
	 * ARB4 */	0,  1,  2,  4, -1, -1, -1, -1,
	/* ARB5 */	0,  1,  2,  3,  4, -1, -1, -1,
	/* ARB6 */	0,  1,  2,  3,  4,  6, -1, -1,
	/* ARB7 */	0,  1,  2,  3,  4,  5,  6, -1,
	/* ARB8 */	0,  1,  2,  3,  4,  5,  6,  7
    };
#define ARB_VERT_LOC(n, v) vert_loc[((n) - 4) * 8 + (v) - 1]

    RT_ARB_CK_MAGIC(arb);


    /* make a local copy of the solid */
    memcpy((char *)&larb, (char *)arb, sizeof(struct rt_arb_internal));

    /*
     * Find the encoded form of the specified permutation, if it
     * exists.
     */
    arglen = strlen(encoded_permutation);
    if (rt_arb_get_cgtype(&type, arb, tol, uvec, svec) == 0) type = 0;
    if (type < 4 || type > 8) {return 1;}
    if (arglen < min_tuple_size[type]) {return 1;}
    face_size = (type == 4) ? 3 : 4;
    if (arglen > face_size) {return 1;}
    vertex = encoded_permutation[0] - '1';
    if ((vertex < 0) || (vertex >= type)) {return 1;}
    p = (type == 4) ? perm4[vertex] :
	(type == 5) ? perm5[vertex] :
	(type == 6) ? perm6[vertex] :
	(type == 7) ? perm7[vertex] : perm8[vertex];
    for (;; ++p) {
	if (*p == 0) {
	    return 1;
	}
	if (bu_strncmp(*p, encoded_permutation, arglen) == 0)
	    break;
    }

    /*
     * Collect the vertices in the specified order
     */
    for (i = 0; i < 8; ++i) {
	char buf[2];

	if ((*p)[i] == '*') {
	    VSETALL(tarb.pt[i], 0);
	} else {
	    sprintf(buf, "%c", (*p)[i]);
	    k = atoi(buf);
	    VMOVE(tarb.pt[i], larb.pt[ARB_VERT_LOC(type, k)]);
	}
    }

    /*
     * Reinstall the permuted vertices back into the temporary buffer,
     * copying redundant vertices as necessary
     *
     *		-------+-------------------------
     *		 Solid |    Redundant storage
     *		  Type | of some of the vertices
     *		-------+-------------------------
     *		 ARB4  |    3=0, 5=6=7=4
     *		 ARB5  |    5=6=7=4
     *		 ARB6  |    5=4, 7=6
     *		 ARB7  |    7=4
     *		 ARB8  |
     *		-------+-------------------------
     */
    for (i = 0; i < 8; i++) {
	VMOVE(larb.pt[i], tarb.pt[i]);
    }
    switch (type) {
	case ARB4:
	    VMOVE(larb.pt[3], larb.pt[0]);
	    /* break intentionally left out */
	case ARB5:
	    VMOVE(larb.pt[5], larb.pt[4]);
	    VMOVE(larb.pt[6], larb.pt[4]);
	    VMOVE(larb.pt[7], larb.pt[4]);
	    break;
	case ARB6:
	    VMOVE(larb.pt[5], larb.pt[4]);
	    VMOVE(larb.pt[7], larb.pt[6]);
	    break;
	case ARB7:
	    VMOVE(larb.pt[7], larb.pt[4]);
	    break;
	case ARB8:
	    break;
	default:
	    {
		return 1;
	    }
    }

    /* copy back to original arb */
    memcpy((char *)arb, (char *)&larb, sizeof(struct rt_arb_internal));

    return 0;
}

int
arb_mirror_face_axis(struct rt_arb_internal *arb, fastf_t peqn[7][4], const int face, const char *axis, const struct bn_tol *tol)
{
    int i, j, k;
    int type;
    int uvec[8], svec[11];
    static int pt[4];
    static int prod;
    static vect_t work;
    struct rt_arb_internal larb;	/* local copy of solid */

    /* check which axis */
    k = -1;
    if (BU_STR_EQUAL(axis, "x"))
	k = 0;
    if (BU_STR_EQUAL(axis, "y"))
	k = 1;
    if (BU_STR_EQUAL(axis, "z"))
	k = 2;
    if (k < 0) {
	return 1;
    }

    work[0] = work[1] = work[2] = 1.0;
    work[k] = -1.0;

    /* make local copy of arb */
    memcpy((char *)&larb, (char *)arb, sizeof(struct rt_arb_internal));
    
    if (rt_arb_get_cgtype(&type, arb, tol, uvec, svec) == 0) type = 0;

    if (type != ARB8 && type != ARB6) {return 1;}

    if (face > 9999 || (face < 1000 && type != ARB6)) {return 1;}

    if (type == ARB6 && face < 1000) {
	/* 3 point face */
	pt[0] = face / 100;
	i = face - (pt[0]*100);
	pt[1] = i / 10;
	pt[2] = i - (pt[1]*10);
	pt[3] = 1;
    } else {
	pt[0] = face / 1000;
	i = face - (pt[0]*1000);
	pt[1] = i / 100;
	i = i - (pt[1]*100);
	pt[2] = i / 10;
	pt[3] = i - (pt[2]*10);
    }

    /* user can input face in any order - will use product of
     * face points to distinguish faces:
     *    product       face
     *       24         1234 for ARB8
     *     1680         5678 for ARB8
     *      252         2367 for ARB8
     *      160         1548 for ARB8
     *      672         4378 for ARB8
     *       60         1256 for ARB8
     *	     10	         125 for ARB6
     *	     72	         346 for ARB6
     */
    prod = 1;
    for (i = 0; i <= 3; i++) {
	prod *= pt[i];
	pt[i]--;
	if (pt[i] > 7) {
	    return 1;
	}
    }

    /* mirror the selected face */
    switch (prod) {

	case 24:   /* mirror face 1234 */
	    if (type == ARB6) {
		return 1;
	    }
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VELMUL(larb.pt[j], larb.pt[i], work);
	    }
	    break;

	case 1680:   /* mirror face 5678 */
	    for (i = 0; i < 4; i++) {
		j = i + 4;
		VELMUL(larb.pt[i], larb.pt[j], work);
	    }
	    break;

	case 60:   /* mirror face 1256 */
	case 10:	/* mirror face 125 of ARB6 */
	    VELMUL(larb.pt[3], larb.pt[0], work);
	    VELMUL(larb.pt[2], larb.pt[1], work);
	    VELMUL(larb.pt[7], larb.pt[4], work);
	    VELMUL(larb.pt[6], larb.pt[5], work);
	    break;

	case 672:   /* mirror face 4378 */
	case 72:	/* mirror face 346 of ARB6 */
	    VELMUL(larb.pt[0], larb.pt[3], work);
	    VELMUL(larb.pt[1], larb.pt[2], work);
	    VELMUL(larb.pt[5], larb.pt[6], work);
	    VELMUL(larb.pt[4], larb.pt[7], work);
	    break;

	case 252:   /* mirror face 2367 */
	    VELMUL(larb.pt[0], larb.pt[1], work);
	    VELMUL(larb.pt[3], larb.pt[2], work);
	    VELMUL(larb.pt[4], larb.pt[5], work);
	    VELMUL(larb.pt[7], larb.pt[6], work);
	    break;

	case 160:   /* mirror face 1548 */
	    VELMUL(larb.pt[1], larb.pt[0], work);
	    VELMUL(larb.pt[5], larb.pt[4], work);
	    VELMUL(larb.pt[2], larb.pt[3], work);
	    VELMUL(larb.pt[6], larb.pt[7], work);
	    break;

	case 120:
	case 180:
	    return 1;
	default:
	    return 1;
    }

    /* redo the plane equations */
    {
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;
	if (rt_arb_calc_planes(&error_msg, &larb, type, peqn, tol)) {
	    return 1;
	}
	bu_vls_free(&error_msg);
    }

    /* copy to original */
    memcpy((char *)arb, (char *)&larb, sizeof(struct rt_arb_internal));

    return 0;
}

int
arb_edit(struct rt_arb_internal *arb, fastf_t peqn[7][4], int edge, int newedge, vect_t pos_model, const struct bn_tol *tol)
{
    int type;
    int pflag = 0;
    int uvec[8], svec[11];
    static int pt1, pt2, bp1, bp2, newp, p1, p2, p3;
    const short *edptr;		/* pointer to arb edit array */
    const short *final;		/* location of points to redo */
    static int i;
    const int *iptr;
    const short earb8[12][18] = earb8_edit_array;
    const short earb7[12][18] = earb7_edit_array;
    const short earb6[10][18] = earb6_edit_array;
    const short earb5[9][18] = earb5_edit_array;
    const short earb4[5][18] = earb4_edit_array;

    RT_ARB_CK_MAGIC(arb);

    if (rt_arb_get_cgtype(&type, arb, tol, uvec, svec) == 0) return 1;

    /* set the pointer */
    switch (type) {
	case ARB4:
	    edptr = &earb4[edge][0];
	    final = &earb4[edge][16];
	    pflag = 1;
	    break;
	case ARB5:
	    edptr = &earb5[edge][0];
	    final = &earb5[edge][16];
	    if (edge == 8)
		pflag = 1;
	    break;
	case ARB6:
	    edptr = &earb6[edge][0];
	    final = &earb6[edge][16];
	    if (edge > 7)
		pflag = 1;
	    break;
	case ARB7:
	    edptr = &earb7[edge][0];
	    final = &earb7[edge][16];
	    if (edge == 11)
		pflag = 1;
	    break;
	case ARB8:
	    edptr = &earb8[edge][0];
	    final = &earb8[edge][16];
	    break;
	default:
	    return 1;
    }


    /* do the arb editing */

    if (pflag) {
	/* moving a point - not an edge */
	VMOVE(arb->pt[edge], pos_model);
	edptr += 4;
    } else {
	vect_t edge_dir;

	/* moving an edge */
	pt1 = *edptr++;
	pt2 = *edptr++;
	/* direction of this edge */
	if (newedge) {
	    /* edge direction comes from edgedir() in pos_model */
	    VMOVE(edge_dir, pos_model);
	    VMOVE(pos_model, arb->pt[pt1]);
	    newedge = 0;
	} else {
	    /* must calculate edge direction */
	    VSUB2(edge_dir, arb->pt[pt2], arb->pt[pt1]);
	}
	if (ZERO(MAGNITUDE(edge_dir)))
	    goto err;
	/* bounding planes bp1, bp2 */
	bp1 = *edptr++;
	bp2 = *edptr++;

	/* move the edge */
	if (mv_edge(arb, pos_model, bp1, bp2, pt1, pt2, edge_dir, tol, peqn)){
	    goto err;
	}
    }

    /* editing is done - insure planar faces */
    /* redo plane eqns that changed */
    newp = *edptr++; 	/* plane to redo */

    if (newp == 9) {
	struct bu_vls error_msg = BU_VLS_INIT_ZERO;
	int arb_calc_ret = 0;
	arb_calc_ret = rt_arb_calc_planes(&error_msg, arb, type, peqn, tol);
	bu_vls_free(&error_msg);
	if (arb_calc_ret) goto err;
    }

    if (newp >= 0 && newp < 6) {
	for (i=0; i<3; i++) {
	    /* redo this plane (newp), use points p1, p2, p3 */
	    p1 = *edptr++;
	    p2 = *edptr++;
	    p3 = *edptr++;
	    if (bn_mk_plane_3pts(peqn[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], tol))
		goto err;

	    /* next plane */
	    if ((newp = *edptr++) == -1 || newp == 8)
		break;
	}
    }
    if (newp == 8) {
	/* special...redo next planes using pts defined in faces */
	const int local_arb_faces[5][24] = rt_arb_faces;
	for (i=0; i<3; i++) {
	    if ((newp = *edptr++) == -1)
		break;
	    iptr = &local_arb_faces[type-4][4*newp];
	    p1 = *iptr++;
	    p2 = *iptr++;
	    p3 = *iptr++;
	    if (bn_mk_plane_3pts(peqn[newp], arb->pt[p1], arb->pt[p2],
				 arb->pt[p3], tol))
		goto err;
	}
    }

    /* the changed planes are all redone push necessary points back
     * into the planes.
     */
    edptr = final;	/* point to the correct location */
    for (i=0; i<2; i++) {
	if ((p1 = *edptr++) == -1)
	    break;
	/* intersect proper planes to define vertex p1 */
	if (rt_arb_3face_intersect(arb->pt[p1], (const plane_t *)peqn, type, p1*3))
	    goto err;
    }

    /* Special case for ARB7: move point 5 .... must recalculate plane
     * 2 = 456
     */
    if (type == ARB7 && pflag) {
	if (bn_mk_plane_3pts(peqn[2], arb->pt[4], arb->pt[5], arb->pt[6], tol))
	    goto err;
    }

    /* carry along any like points */
    switch (type) {
	case ARB8:
	    break;

	case ARB7:
	    VMOVE(arb->pt[7], arb->pt[4]);
	    break;

	case ARB6:
	    VMOVE(arb->pt[5], arb->pt[4]);
	    VMOVE(arb->pt[7], arb->pt[6]);
	    break;

	case ARB5:
	    for (i=5; i<8; i++)
		VMOVE(arb->pt[i], arb->pt[4]);
	    break;

	case ARB4:
	    VMOVE(arb->pt[3], arb->pt[0]);
	    for (i=5; i<8; i++)
		VMOVE(arb->pt[i], arb->pt[4]);
	    break;
    }

    return 0;		/* OK */

 err:
    return 1;		/* BAD */

} 

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
