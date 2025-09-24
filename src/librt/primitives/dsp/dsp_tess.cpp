/*                   D S P _ T E S S . C P P
 * BRL-CAD
 *
 * Copyright (c) 1999-2025 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/dsp/dsp_tess.cpp
 *
 * DSP tessellation logic
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include "bnetwork.h"

#include "bu/cv.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/db4.h"
#include "bv/plot3.h"

/* private header */
#include "./dsp.h"

/*
 * Determine the cut direction for a DSP cell. This routine is used by
 * rt_dsp_tess(). It somewhat duplicates code from permute_cell(),
 * which is a bad thing, but permute_cell() had other side effects not
 * desired here.
 *
 * inputs:
 * dsp_ip - pointer to the rt_dsp_internal struct for this DSP
 * x - the DSP cell x-coord of the lower left corner of the cell
 * y - the DSP cell y-coord of the lower left corner of the cell
 * xlim - the maximum value of the DSP x coordinates
 * ylim - the maximum value of the DSP y coordinates
 * return:
 * the direction for cutting this cell.
 * DSP_CUT_DIR_llUR - lower left to upper right
 * DSP_CUT_DIR_ULlr - upper right to lower left
 */
static int
get_cut_dir(struct rt_dsp_internal *dsp_ip, int x, int y, int xlim, int ylim)
{
/*
 * height array contains DSP values:
 * height[0] is at (x, y)
 * height[1] is at (x+1, y)
 * height[2] is at (x+1, y+1)
 * height[3] is at (x, y+1)
 * height[4] is at (x-1, y-1)
 * height[5] is at (x+2, y-1)
 * height[6] is at (x+2, y+2)
 * height[7] is at (x-1, y+2)
 *
 * 7         6
 *  \       /
 *   \     /
 *    3---2
 *    |   |
 *    |   |
 *    0---1
 *   /     \
 *  /       \
 * 4         5
 *
 * (0, 1, 2, 3) is the cell of interest.
 */
    int height[8];
    int xx, yy;
    fastf_t c02, c13;  /* curvature in direction 0<->2, and 1<->3 */

    if (dsp_ip->dsp_cuttype != DSP_CUT_DIR_ADAPT) {
	/* not using adaptive cut type, so just return the cut type */
	return dsp_ip->dsp_cuttype;
    }

    /* fill in the height array */
    xx = x;
    yy = y;
    height[0] = DSP(dsp_ip, xx, yy);
    xx = x+1;
    if (xx > xlim) xx = xlim;
    height[1] = DSP(dsp_ip, xx, yy);
    yy = y+1;
    if (yy > ylim) yy = ylim;
    height[2] = DSP(dsp_ip, xx, yy);
    xx = x;
    height[3] = DSP(dsp_ip, xx, yy);
    xx = x-1;
    if (xx < 0) xx = 0;
    yy = y-1;
    if (yy < 0) yy = 0;
    height[4] = DSP(dsp_ip, xx, yy);
    xx = x+2;
    if (xx > xlim) xx = xlim;
    height[5] = DSP(dsp_ip, xx, yy);
    yy = y+2;
    if (yy > ylim) yy = ylim;
    height[6] = DSP(dsp_ip, xx, yy);
    xx = x-1;
    if (xx < 0) xx = 0;
    height[7] = DSP(dsp_ip, xx, yy);

    /* compute curvature along the 0<->2 direction */
    c02 = abs(height[2] + height[4] - 2*height[0]) + abs(height[6] + height[0] - 2*height[2]);


    /* compute curvature along the 1<->3 direction */
    c13 = abs(height[3] + height[5] - 2*height[1]) + abs(height[7] + height[1] - 2*height[3]);

    if (c02 < c13) {
	/* choose the 0<->2 direction */
	return DSP_CUT_DIR_llUR;
    } else {
	/* choose the other direction */
	return DSP_CUT_DIR_ULlr;
    }
}


/* a macro to check if these vertices can produce a valid face */
#define CHECK_VERTS(_v) \
     ((*_v[0] != *_v[1]) || (*_v[0] == NULL)) && \
     ((*_v[0] != *_v[2]) || (*_v[2] == NULL)) && \
     ((*_v[1] != *_v[2]) || (*_v[1] == NULL))

/**
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
extern "C" int
rt_dsp_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    struct rt_dsp_internal *dsp_ip;
    struct shell *s;
    int xlim;
    int ylim;
    int x, y;
    point_t pt[4];
    point_t tmp_pt;
    int base_vert_count;
    struct vertex **base_verts;
    struct vertex **verts[3];
    struct vertex *hole_verts[3];
    struct faceuse *fu;
    struct faceuse *base_fu;
    struct vertex **strip1Verts;
    struct vertex **strip2Verts;
    int base_vert_no1;
    int base_vert_no2;
    int has_holes = 0;
    struct bu_list *vlfree = &rt_vlfree;

    if (RT_G_DEBUG & RT_DEBUG_HF)
	bu_log("rt_dsp_tess()\n");

    /* do a bunch of checks to make sure all is well */

    RT_CK_DB_INTERNAL(ip);
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    if (!dsp_ip->dsp_mp) {
		bu_log("WARNING: Cannot find data file for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data file [%s] not found or empty\n", bu_vls_addr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data file not found or not specified\n");
		}
		return -1;
	    }
	    break;
	case RT_DSP_SRC_OBJ:
	    if (!dsp_ip->dsp_bip) {
		bu_log("WARNING: Cannot find data object for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data object [%s] not found or empty\n", bu_vls_addr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data object not found or not specified\n");
		}
		return -1;
	    }
	    RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
	    RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
	    break;
    }


    xlim = dsp_ip->dsp_xcnt - 1;
    ylim = dsp_ip->dsp_ycnt - 1;

    /* Base_verts will contain the vertices for the base face ordered
     * correctly to create the base face.
     * Cannot simply use the four corners, because that would not
     * create a valid NMG.
     * base_verts[0] is at (0, 0)
     * base_verts[ylim] is at (0, ylim)
     * base_verts[ylim+xlim] is at (xlim, ylim)
     * base_verts[2*ylim+xlim] is at (xlim, 0)
     * base_verts[2*ylim+2*xlim-x] is at (x, 0)
     *
     * strip1Verts and strip2Verts are temporary storage for vertices
     * along the top of the dsp. For each strip of triangles at a
     * given x value, strip1Verts[y] is the vertex at (x, y, h) and
     * strip2Verts[y] is the vertex at (x+1, y, h), where h is the DSP
     * value at that point.  After each strip of faces is created,
     * strip2Verts is copied to strip1Verts and strip2Verts is set to
     * all NULLs.
     */

    /* malloc space for the vertices */
    base_vert_count = 2*xlim + 2*ylim;
    base_verts = (struct vertex **)bu_calloc(base_vert_count, sizeof(struct vertex *), "base verts");
    strip1Verts = (struct vertex **)bu_calloc(ylim+1, sizeof(struct vertex *), "strip1Verts");
    strip2Verts = (struct vertex **)bu_calloc(ylim+1, sizeof(struct vertex *), "strip2Verts");

    /* Make region, empty shell, vertex */
    *r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    /* make the base face */
    base_fu = nmg_cface(s, base_verts, base_vert_count);

    /* assign geometry to the base_verts */
    /* start with x=0 edge */
    for (y = 0; y <= ylim; y++) {
	VSET(tmp_pt, 0, y, 0);
	MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	nmg_vertex_gv(base_verts[y], pt[0]);
    }
    /* now do y=ylim edge */
    for (x = 1; x <= xlim; x++) {
	VSET(tmp_pt, x, ylim, 0);
	MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	nmg_vertex_gv(base_verts[ylim+x], pt[0]);
    }
    /* now do x=xlim edge */
    for (y = 0; y < ylim; y++) {
	VSET(tmp_pt, xlim, y, 0);
	MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	nmg_vertex_gv(base_verts[2*ylim+xlim-y], pt[0]);
    }
    /* now do y=0 edge */
    for (x = 1; x < xlim; x++) {
	VSET(tmp_pt, x, 0, 0);
	MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	nmg_vertex_gv(base_verts[2*(xlim+ylim)-x], pt[0]);
    }
    if (nmg_fu_planeeqn(base_fu, tol) < 0) {
	bu_log("Failed to make base face\n");
	bu_free(base_verts, "base verts");
	bu_free(strip1Verts, "strip 1 verts");
	bu_free(strip2Verts, "strip 2 verts");
	return -1; /* FAIL */
    }

    /* if a displacement on the edge (x=0) is zero then strip1Verts at
     * that point is the corresponding base_vert
     */
    for (y = 0; y <= ylim; y++) {
	if (DSP(dsp_ip, 0, y) == 0) {
	    strip1Verts[y] = base_verts[y];
	}
    }

    /* make faces along x=0 plane */
    for (y= 1; y <= ylim; y++) {
	verts[0] = &base_verts[y-1];
	verts[1] = &strip1Verts[y-1];
	verts[2] = &strip1Verts[y];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (y == 1 && strip1Verts[0]->vg_p == NULL) {
		VSET(tmp_pt, 0, 0, DSP(dsp_ip, 0, 0));
		MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
		nmg_vertex_gv(strip1Verts[0], pt[0]);
	    }
	    if (strip1Verts[y]->vg_p == NULL) {
		VSET(tmp_pt, 0, y, DSP(dsp_ip, 0, y));
		MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
		nmg_vertex_gv(strip1Verts[y], pt[0]);
	    }
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make x=0 face at y=%d\n", y);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}
	verts[0] = &base_verts[y-1];
	verts[1] = &strip1Verts[y];
	verts[2] = &base_verts[y];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (strip1Verts[y]->vg_p == NULL) {
		VSET(tmp_pt, 0, y, DSP(dsp_ip, 0, y));
		MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
		nmg_vertex_gv(strip1Verts[y], pt[0]);
	    }
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make x=0 face at y=%d\n", y);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}
    }

    /* make each strip of triangles. Make two triangles for each cell
     * (x, y)<->(x+1, y+1). Also make the vertical faces at y=0 and
     * y=ylim.
     */
    for (x = 0; x < xlim; x++) {
	/* set strip2Verts to base_verts values where the strip2Vert
	 * is above a base_vert and DSP value is 0
	 */
	if ((x+1) == xlim) {
	    for (y = 0; y <= ylim; y++) {
		if (DSP(dsp_ip, xlim, y) == 0) {
		    strip2Verts[y] = base_verts[2*ylim + xlim - y];
		}
	    }
	} else {
	    if (DSP(dsp_ip, x+1, 0) == 0) {
		strip2Verts[0] = base_verts[2*(ylim+xlim)-(x+1)];
	    }
	    if (DSP(dsp_ip, x+1, ylim) == 0) {
		strip2Verts[ylim] = base_verts[ylim+x+1];
	    }
	}

	/* make the faces at y=0 for this strip */
	if (x == 0) {
	    base_vert_no1 = 0;
	    base_vert_no2 = 2*(ylim+xlim)-1;
	} else {
	    base_vert_no1 = 2*(ylim + xlim) - x;
	    base_vert_no2 = base_vert_no1 - 1;
	}

	verts[0] = &base_verts[base_vert_no1];
	verts[1] = &strip2Verts[0];
	verts[2] = &strip1Verts[0];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    VSET(tmp_pt, x+1, 0, DSP(dsp_ip, x+1, 0));
	    MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	    nmg_vertex_gv(strip2Verts[0], pt[0]);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make first face at x=%d, y=%d\n", x, 0);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}

	verts[0] = &base_verts[base_vert_no1];
	verts[1] = &base_verts[base_vert_no2];
	verts[2] = &strip2Verts[0];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make second face at x=%d, y=%d\n", x, 0);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	    if (strip2Verts[0]->vg_p == NULL) {
		VSET(tmp_pt, x+1, 0, DSP(dsp_ip, x+1, 0));
		MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
		nmg_vertex_gv(strip2Verts[0], pt[0]);
	    }
	}

	/* make the top faces for this strip */
	for (y = 0; y < ylim; y++) {
	    int dir;
	    /* get the cut direction for this cell */
	    dir = get_cut_dir(dsp_ip, x, y, xlim, ylim);

	    if (dir == DSP_CUT_DIR_llUR) {
		verts[0] = &strip1Verts[y];
		verts[1] = &strip2Verts[y];
		verts[2] = &strip2Verts[y+1];
		if (CHECK_VERTS(verts)) {
		    if (DSP(dsp_ip, x, y) == 0 && DSP(dsp_ip, x+1, y) == 0 && DSP(dsp_ip, x+1, y+1) == 0) {
			/* make a hole, instead of a face */
			hole_verts[0] = strip1Verts[y];
			hole_verts[1] = strip2Verts[y];
			hole_verts[2] = strip2Verts[y+1];
			nmg_add_loop_to_face(s, base_fu, hole_verts, 3, OT_OPPOSITE);
			has_holes = 1;
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(hole_verts[2], pt[0]);
			strip2Verts[y+1] = hole_verts[2];
		    } else {
			fu = nmg_cmface(s, verts, 3);
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(strip2Verts[y+1], pt[0]);
			if (nmg_fu_planeeqn(fu, tol) < 0) {
			    bu_log("Failed to make first top face at x=%d, y=%d\n", x, y);
			    bu_free(base_verts, "base verts");
			    bu_free(strip1Verts, "strip 1 verts");
			    bu_free(strip2Verts, "strip 2 verts");
			    return -1; /* FAIL */
			}
		    }
		}
	    } else {
		verts[0] = &strip1Verts[y+1];
		verts[1] = &strip1Verts[y];
		verts[2] = &strip2Verts[y];
		if (CHECK_VERTS(verts)) {
		    if (DSP(dsp_ip, x, y+1) == 0 && DSP(dsp_ip, x, y) == 0 && DSP(dsp_ip, x+1, y) == 0) {
			/* make a hole, instead of a face */
			hole_verts[0] = strip1Verts[y+1];
			hole_verts[1] = strip1Verts[y];
			hole_verts[2] = strip2Verts[y];
			nmg_add_loop_to_face(s, base_fu, hole_verts, 3, OT_OPPOSITE);
			has_holes = 1;
		    } else {
			fu = nmg_cmface(s, verts, 3);
			if (nmg_fu_planeeqn(fu, tol) < 0) {
			    bu_log("Failed to make first top face at x=%d, y=%d\n", x, y);
			    bu_free(base_verts, "base verts");
			    bu_free(strip1Verts, "strip 1 verts");
			    bu_free(strip2Verts, "strip 2 verts");
			    return -1; /* FAIL */
			}
		    }
		}
	    }


	    if (dir == DSP_CUT_DIR_llUR) {
		verts[0] = &strip1Verts[y];
		verts[1] = &strip2Verts[y+1];
		verts[2] = &strip1Verts[y+1];
		if (CHECK_VERTS(verts)) {
		    if (DSP(dsp_ip, x, y) == 0 && DSP(dsp_ip, x+1, y+1) == 0 && DSP(dsp_ip, x, y+1) == 0) {
			/* make a hole, instead of a face */
			hole_verts[0] = strip1Verts[y];
			hole_verts[1] = strip2Verts[y+1];
			hole_verts[2] = strip1Verts[y+1];
			nmg_add_loop_to_face(s, base_fu, hole_verts, 3, OT_OPPOSITE);
			has_holes = 1;
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(hole_verts[1], pt[0]);
			strip2Verts[y+1] = hole_verts[1];
		    } else {
			fu = nmg_cmface(s, verts, 3);
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(strip2Verts[y+1], pt[0]);
			if (nmg_fu_planeeqn(fu, tol) < 0) {
			    bu_log("Failed to make second top face at x=%d, y=%d\n", x, y);
			    bu_free(base_verts, "base verts");
			    bu_free(strip1Verts, "strip 1 verts");
			    bu_free(strip2Verts, "strip 2 verts");
			    return -1; /* FAIL */
			}
		    }
		}
	    } else {
		verts[0] = &strip2Verts[y];
		verts[1] = &strip2Verts[y+1];
		verts[2] = &strip1Verts[y+1];
		if (CHECK_VERTS(verts)) {
		    if (DSP(dsp_ip, x+1, y) == 0 && DSP(dsp_ip, x+1, y+1) == 0 && DSP(dsp_ip, x, y+1) == 0) {
			/* make a hole, instead of a face */
			hole_verts[0] = strip2Verts[y];
			hole_verts[1] = strip2Verts[y+1];
			hole_verts[2] = strip1Verts[y+1];
			nmg_add_loop_to_face(s, base_fu, hole_verts, 3, OT_OPPOSITE);
			has_holes = 1;
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(hole_verts[1], pt[0]);
			strip2Verts[y+1] = hole_verts[1];
		    } else {
			fu = nmg_cmface(s, verts, 3);
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(strip2Verts[y+1], pt[0]);
			if (nmg_fu_planeeqn(fu, tol) < 0) {
			    bu_log("Failed to make second top face at x=%d, y=%d\n", x, y);
			    bu_free(base_verts, "base verts");
			    bu_free(strip1Verts, "strip 1 verts");
			    bu_free(strip2Verts, "strip 2 verts");
			    return -1; /* FAIL */
			}
		    }
		}
	    }
	}

	/* make the faces at the y=ylim plane for this strip */
	verts[0] = &strip1Verts[ylim];
	verts[1] = &strip2Verts[ylim];
	verts[2] = &base_verts[ylim+x+1];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make first face at x=%d, y=ylim\n", x);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}

	verts[0] = &base_verts[ylim+x+1];
	verts[1] = &base_verts[ylim+x];
	verts[2] = &strip1Verts[ylim];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make second face at x=%d, y=ylim\n", x);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}

	/* copy strip2 to strip1, set strip2 to all NULLs */
	for (y = 0; y <= ylim; y++) {
	    strip1Verts[y] = strip2Verts[y];
	    strip2Verts[y] = (struct vertex *)NULL;
	}
    }

    /* make faces at x=xlim plane */
    for (y = 0; y < ylim; y++) {
	base_vert_no1 = 2*ylim+xlim-y;
	verts[0] = &base_verts[base_vert_no1];
	verts[1] = &base_verts[base_vert_no1-1];
	verts[2] = &strip1Verts[y];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make first face at x=xlim, y=%d\n", y);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}

	verts[0] = &strip1Verts[y];
	verts[1] = &base_verts[base_vert_no1-1];
	verts[2] = &strip1Verts[y+1];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make second face at x=xlim, y=%d\n", y);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}
    }


    bu_free(base_verts, "base verts");
    bu_free(strip1Verts, "strip 1 verts");
    bu_free(strip2Verts, "strip 2 verts");

    if (has_holes) {
	/* do a bunch of joining and splitting of touching loops to
	 * get the base face in a state that the triangulator can
	 * handle
	 */
	struct loopuse *lu;
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    nmg_join_touchingloops(lu);
	}
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    if (lu->orientation != OT_UNSPEC) continue;
	    nmg_lu_reorient(lu);
	}
	if (!nmg_kill_cracks(s))
	    return -1;

	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    nmg_split_touchingloops(lu, tol);
	}
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    if (lu->orientation != OT_UNSPEC) continue;
	    nmg_lu_reorient(lu);
	}
	if (!nmg_kill_cracks(s))
	    return -1;

	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    nmg_join_touchingloops(lu);
	}
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    if (lu->orientation != OT_UNSPEC) continue;
	    nmg_lu_reorient(lu);
	}
	if (!nmg_kill_cracks(s))
	    return -1;
    }

    /* Mark edges as real */
    (void)nmg_mark_edges_real(&s->l.magic, vlfree);

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

    /* sanity check */
    nmg_make_faces_within_tol(s, vlfree, tol);

    return 0;
}


/** @} */

