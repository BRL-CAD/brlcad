/*                       E X T R U D E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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
/** @file primitives/extrude/extrude.c
 *
 * Provide support for solids of extrusion.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bnetwork.h"

#include "vmath.h"
#include "bu/cv.h"
#include "bu/debug.h"
#include "nmg.h"
#include "rt/db4.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../../librt_private.h"


extern int seg_to_vlist(struct bu_list *vlfree, struct bu_list *vhead, const struct bg_tess_tol *ttol, point_t V,
			vect_t u_vec, vect_t v_vec, struct rt_sketch_internal *sketch_ip, void *seg);

extern void rt_sketch_surf_area(fastf_t *area, const struct rt_db_internal *ip);
extern void rt_sketch_centroid(point_t *cent, const struct rt_db_internal *ip);

struct extrude_specific {
    mat_t rot, irot;	/* rotation and translation to get extrusion vector in +z direction with V at origin */
    vect_t unit_h;	/* unit vector in direction of extrusion vector */
    vect_t u_vec;	/* u vector rotated and projected */
    vect_t v_vec;	/* v vector rotated and projected */
    fastf_t uv_scale;	/* length of original, untransformed u_vec */
    vect_t rot_axis;	/* axis of rotation for rotation matrix */
    vect_t perp;	/* vector in pl1_rot plane and normal to rot_axis */
    plane_t pl1, pl2;	/* plane equations of the top and bottom planes (not rotated) */
    plane_t pl1_rot;	/* pl1 rotated by rot */
    point_t *verts;	/* sketch vertices projected onto a plane normal to extrusion vector */
    struct rt_curve crv;	/* copy of the referenced curve */
};


static struct bn_tol extr_tol = {
    /* a fake tolerance structure for the intersection routines */
    BN_TOL_MAGIC,
    RT_LEN_TOL,
    RT_LEN_TOL*RT_LEN_TOL,
    0.0,
    1.0
};


#define MAX_HITS 64

/* defines for surf_no in the hit struct (a negative surf_no indicates an exit point) */
#define TOP_FACE	1	/* extruded face */
#define BOTTOM_FACE	2	/* face in uv-plane */
#define LINE_SEG	3
#define CARC_SEG	4
#define NURB_SEG	5
#define BEZIER_SEG	6

/* defines for loop classification */
#define UNKNOWN		0
#define A_IN_B		1
#define B_IN_A		2
#define DISJOINT	3

#define LOOPA		1
#define LOOPB		2

/**
 * Calculate a bounding RPP for an extruded sketch
 */
int
rt_extrude_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *tol)
{
    struct rt_extrude_internal *eip;
    struct extrude_specific extr;
    struct rt_sketch_internal *skt;
    vect_t tmp, xyz[3];
    fastf_t tmp_f, ldir[3];
    size_t i, j;
    size_t vert_count;
    size_t curr_vert;

    eip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(eip);
    skt = eip->skt;
    RT_SKETCH_CK_MAGIC(skt);

    /* make sure the sketch is valid */
    if (rt_check_curve(&skt->curve, skt, 1)) {
	bu_log("ERROR: referenced sketch (%s) is bad!\n",
	       eip->sketch_name);
	return -1;
    }

    memset(&extr, 0, sizeof(struct extrude_specific));

    VMOVE(extr.unit_h, eip->h);
    VUNITIZE(extr.unit_h);

    /* the length of the u_vec is used for scaling radii of circular
     * arcs the u_vec and the v_vec must have the same length
     */
    extr.uv_scale = MAGNITUDE(eip->u_vec);

    /* build a transformation matrix to rotate extrusion vector to
     * z-axis
     */
    VSET(tmp, 0, 0, 1);
    bn_mat_fromto(extr.rot, eip->h, tmp, tol);

    /* and translate to origin */
    extr.rot[MDX] = -VDOT(eip->V, &extr.rot[0]);
    extr.rot[MDY] = -VDOT(eip->V, &extr.rot[4]);
    extr.rot[MDZ] = -VDOT(eip->V, &extr.rot[8]);

    /* and save the inverse */
    bn_mat_inv(extr.irot, extr.rot);

    /* calculate plane equations of top and bottom planes */
    VCROSS(extr.pl1, eip->u_vec, eip->v_vec);
    VUNITIZE(extr.pl1);
    extr.pl1[W] = VDOT(extr.pl1, eip->V);
    VMOVE(extr.pl2, extr.pl1);
    VADD2(tmp, eip->V, eip->h);
    extr.pl2[W] = VDOT(extr.pl2, tmp);

    vert_count = skt->vert_count;
    /* count how many additional vertices we will need for arc centers */
    for (i = 0; i < skt->curve.count; i++) {
	struct carc_seg *csg=(struct carc_seg *)skt->curve.segment[i];

	if (csg->magic != CURVE_CARC_MAGIC)
	    continue;

	if (csg->radius <= 0.0)
	    continue;

	vert_count++;
    }

    /* apply the rotation matrix to all the vertices, and start
     * bounding box calculation
     */
    if (vert_count) {
	extr.verts = (point_t *)bu_calloc(vert_count, sizeof(point_t),
		"extr.verts");
    }
    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    for (i = 0; i < skt->vert_count; i++) {
	VJOIN2(tmp, eip->V, skt->verts[i][0], eip->u_vec, skt->verts[i][1], eip->v_vec);
	VMINMAX((*min), (*max), tmp);
	MAT4X3PNT(extr.verts[i], extr.rot, tmp);
	VADD2(tmp, tmp, eip->h);
	VMINMAX((*min), (*max), tmp);
    }
    curr_vert = skt->vert_count;

    /* and the u, v vectors */
    MAT4X3VEC(extr.u_vec, extr.rot, eip->u_vec);
    MAT4X3VEC(extr.v_vec, extr.rot, eip->v_vec);

    /* calculate the rotated pl1 */
    VCROSS(extr.pl1_rot, extr.u_vec, extr.v_vec);
    VUNITIZE(extr.pl1_rot);
    extr.pl1_rot[W] = VDOT(extr.pl1_rot, extr.verts[0]);

    VSET(tmp, 0, 0, 1);
    tmp_f = VDOT(tmp, extr.unit_h);
    if (tmp_f < 0.0)
	tmp_f = -tmp_f;
    tmp_f -= 1.0;
    if (NEAR_ZERO(tmp_f, SQRT_SMALL_FASTF)) {
	VSET(extr.rot_axis, 1.0, 0.0, 0.0);
	VSET(extr.perp, 0.0, 1.0, 0.0);
    } else {
	VCROSS(extr.rot_axis, tmp, extr.unit_h);
	VUNITIZE(extr.rot_axis);
	if (MAGNITUDE(extr.rot_axis) < SQRT_SMALL_FASTF) {
	    VSET(extr.rot_axis, 1.0, 0.0, 0.0);
	    VSET(extr.perp, 0.0, 1.0, 0.0);
	} else {
	    VCROSS(extr.perp, extr.rot_axis, extr.pl1_rot);
	    VUNITIZE(extr.perp);
	}
    }

    /* copy the curve */
    rt_copy_curve(&extr.crv, &skt->curve);

    VSET(xyz[X], 1, 0, 0);
    VSET(xyz[Y], 0, 1, 0);
    VSET(xyz[Z], 0, 0, 1);

    for (i=X; i<=Z; i++) {
	VCROSS(tmp, extr.unit_h, xyz[i]);
	ldir[i] = MAGNITUDE(tmp);
    }

    /* if any part of the curve is a circular arc, the arc may extend
     * beyond the listed vertices
     */
    for (i = 0; i < skt->curve.count; i++) {
	struct carc_seg *csg=(struct carc_seg *)skt->curve.segment[i];
	struct carc_seg *csg_extr=(struct carc_seg *)extr.crv.segment[i];
	point_t center;

	if (csg->magic != CURVE_CARC_MAGIC)
	    continue;

	if (csg->radius <= 0.0) {
	    /* full circle */
	    point_t start;
	    fastf_t radius;

	    csg_extr->center = csg->end;
	    VJOIN2(start, eip->V, skt->verts[csg->start][0], eip->u_vec, skt->verts[csg->start][1], eip->v_vec);
	    VJOIN2(center, eip->V, skt->verts[csg->end][0], eip->u_vec, skt->verts[csg->end][1], eip->v_vec);
	    VSUB2(tmp, start, center);
	    radius = MAGNITUDE(tmp);
	    csg_extr->radius = -radius;	/* need the correct magnitude for normal calculation */

	    for (j=X; j<=Z; j++) {
		tmp_f = radius * ldir[j];
		VJOIN1(tmp, center, tmp_f, xyz[j]);
		VMINMAX((*min), (*max), tmp);
		VADD2(tmp, tmp, eip->h);
		VMINMAX((*min), (*max), tmp);

		VJOIN1(tmp, center, -tmp_f, xyz[j]);
		VMINMAX((*min), (*max), tmp);
		VADD2(tmp, tmp, eip->h);
		VMINMAX((*min), (*max), tmp);
	    }
	} else {
	    /* circular arc */
	    point_t start, end, mid;
	    vect_t s_to_m;
	    vect_t bisector;
	    fastf_t dist;
	    fastf_t magsq_s2m;

	    VJOIN2(start, eip->V, skt->verts[csg->start][0], eip->u_vec, skt->verts[csg->start][1], eip->v_vec);
	    VJOIN2(end, eip->V, skt->verts[csg->end][0], eip->u_vec, skt->verts[csg->end][1], eip->v_vec);
	    VBLEND2(mid, 0.5, start, 0.5, end);
	    VSUB2(s_to_m, mid, start);
	    VCROSS(bisector, extr.pl1, s_to_m);
	    VUNITIZE(bisector);
	    magsq_s2m = MAGSQ(s_to_m);
	    csg_extr->radius = csg->radius * extr.uv_scale;
	    if (magsq_s2m > csg_extr->radius*csg_extr->radius) {
		fastf_t max_radius;

		max_radius = sqrt(magsq_s2m);
		if (NEAR_EQUAL(max_radius, csg_extr->radius, RT_LEN_TOL)) {
		    csg_extr->radius = max_radius;
		} else {
		    bu_log("Impossible radius for circular arc in extrusion - is %g, cannot be more than %g!\n",
			   csg_extr->radius, sqrt(magsq_s2m));
		    bu_log("Difference is %g\n", max_radius - csg->radius);
		    return -1;
		}
	    }
	    dist = sqrt(csg_extr->radius*csg_extr->radius - magsq_s2m);

	    /* save arc center */
	    if (csg->center_is_left) {
		VJOIN1(center, mid, dist, bisector);
	    } else {
		VJOIN1(center, mid, -dist, bisector);
	    }
	    MAT4X3PNT(extr.verts[curr_vert], extr.rot, center);
	    csg_extr->center = curr_vert;
	    curr_vert++;

	    for (j=X; j<=Z; j++) {
		tmp_f = csg_extr->radius * ldir[j];
		VJOIN1(tmp, center, tmp_f, xyz[j]);
		VMINMAX((*min), (*max), tmp);
		VADD2(tmp, tmp, eip->h);
		VMINMAX((*min), (*max), tmp);

		VJOIN1(tmp, center, -tmp_f, xyz[j]);
		VMINMAX((*min), (*max), tmp);
		VADD2(tmp, tmp, eip->h);
		VMINMAX((*min), (*max), tmp);
	    }
	}
    }

    if (extr.verts)
	 bu_free((char *)extr.verts, "extrude->verts");
    rt_curve_free(&(extr.crv));

    return 0;              /* OK */
}


/**
 * Calculate the volume of an extruded object
 */
void
rt_extrude_volume(fastf_t *vol, const struct rt_db_internal *ip)
{
    struct rt_extrude_internal *eip;
    struct rt_sketch_internal *skt;
    struct rt_db_internal db_skt;
    fastf_t area;
    fastf_t h_norm;
    fastf_t u_norm;
    fastf_t v_norm;
    eip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(eip);
    skt = eip->skt;
    RT_SKETCH_CK_MAGIC(skt);

    RT_DB_INTERNAL_INIT(&db_skt);
    db_skt.idb_ptr = (void *)skt;

    rt_sketch_surf_area(&area, &db_skt);

    h_norm = MAGNITUDE(eip->h);
    u_norm = MAGNITUDE(eip->u_vec);
    v_norm = MAGNITUDE(eip->v_vec);

    *vol = area * h_norm * u_norm * v_norm;
}


/**
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid EXTRUDE, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 EXTRUDE is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct extrude_specific is created, and its address is stored in
 * stp->st_specific for use by extrude_shot().
 */
int
rt_extrude_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_extrude_internal *eip;
    struct extrude_specific *extr;
    struct rt_sketch_internal *skt;
    vect_t tmp, xyz[3];
    fastf_t tmp_f, ldir[3];
    size_t i, j;
    size_t vert_count;
    size_t curr_vert;

    if (rtip) RT_CK_RTI(rtip);

    eip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(eip);
    skt = eip->skt;
    RT_SKETCH_CK_MAGIC(skt);

    /* make sure the sketch is valid */
    if (rt_check_curve(&skt->curve, skt, 1)) {
	bu_log("ERROR: referenced sketch (%s) is bad!\n",
	       eip->sketch_name);
	return -1;
    }

    BU_GET(extr, struct extrude_specific);
    stp->st_specific = (void *)extr;

    VMOVE(extr->unit_h, eip->h);
    VUNITIZE(extr->unit_h);

    /* the length of the u_vec is used for scaling radii of circular
     * arcs the u_vec and the v_vec must have the same length
     */
    extr->uv_scale = MAGNITUDE(eip->u_vec);

    /* build a transformation matrix to rotate extrusion vector to
     * z-axis
     */
    VSET(tmp, 0, 0, 1);
    bn_mat_fromto(extr->rot, eip->h, tmp, &rtip->rti_tol);

    /* and translate to origin */
    extr->rot[MDX] = -VDOT(eip->V, &extr->rot[0]);
    extr->rot[MDY] = -VDOT(eip->V, &extr->rot[4]);
    extr->rot[MDZ] = -VDOT(eip->V, &extr->rot[8]);

    /* and save the inverse */
    bn_mat_inv(extr->irot, extr->rot);

    /* calculate plane equations of top and bottom planes */
    VCROSS(extr->pl1, eip->u_vec, eip->v_vec);
    VUNITIZE(extr->pl1);
    extr->pl1[W] = VDOT(extr->pl1, eip->V);
    VMOVE(extr->pl2, extr->pl1);
    VADD2(tmp, eip->V, eip->h);
    extr->pl2[W] = VDOT(extr->pl2, tmp);

    vert_count = skt->vert_count;
    /* count how many additional vertices we will need for arc centers */
    for (i = 0; i < skt->curve.count; i++) {
	struct carc_seg *csg=(struct carc_seg *)skt->curve.segment[i];

	if (csg->magic != CURVE_CARC_MAGIC)
	    continue;

	if (csg->radius <= 0.0)
	    continue;

	vert_count++;
    }

    /* apply the rotation matrix to all the vertices, and start
     * bounding box calculation
     */
    if (vert_count) {
	extr->verts = (point_t *)bu_calloc(vert_count, sizeof(point_t),
		"extr->verts");
    }
    VSETALL(stp->st_min, INFINITY);
    VSETALL(stp->st_max, -INFINITY);

    for (i = 0; i < skt->vert_count; i++) {
	VJOIN2(tmp, eip->V, skt->verts[i][0], eip->u_vec, skt->verts[i][1], eip->v_vec);
	VMINMAX(stp->st_min, stp->st_max, tmp);
	MAT4X3PNT(extr->verts[i], extr->rot, tmp);
	VADD2(tmp, tmp, eip->h);
	VMINMAX(stp->st_min, stp->st_max, tmp);
    }
    curr_vert = skt->vert_count;

    /* and the u, v vectors */
    MAT4X3VEC(extr->u_vec, extr->rot, eip->u_vec);
    MAT4X3VEC(extr->v_vec, extr->rot, eip->v_vec);

    /* calculate the rotated pl1 */
    VCROSS(extr->pl1_rot, extr->u_vec, extr->v_vec);
    VUNITIZE(extr->pl1_rot);
    extr->pl1_rot[W] = VDOT(extr->pl1_rot, extr->verts[0]);

    VSET(tmp, 0, 0, 1);
    tmp_f = VDOT(tmp, extr->unit_h);
    if (tmp_f < 0.0)
	tmp_f = -tmp_f;
    tmp_f -= 1.0;
    if (NEAR_ZERO(tmp_f, SQRT_SMALL_FASTF)) {
	VSET(extr->rot_axis, 1.0, 0.0, 0.0);
	VSET(extr->perp, 0.0, 1.0, 0.0);
    } else {
	VCROSS(extr->rot_axis, tmp, extr->unit_h);
	VUNITIZE(extr->rot_axis);
	if (MAGNITUDE(extr->rot_axis) < SQRT_SMALL_FASTF) {
	    VSET(extr->rot_axis, 1.0, 0.0, 0.0);
	    VSET(extr->perp, 0.0, 1.0, 0.0);
	} else {
	    VCROSS(extr->perp, extr->rot_axis, extr->pl1_rot);
	    VUNITIZE(extr->perp);
	}
    }

    /* copy the curve */
    rt_copy_curve(&extr->crv, &skt->curve);

    VSET(xyz[X], 1, 0, 0);
    VSET(xyz[Y], 0, 1, 0);
    VSET(xyz[Z], 0, 0, 1);

    for (i=X; i<=Z; i++) {
	VCROSS(tmp, extr->unit_h, xyz[i]);
	ldir[i] = MAGNITUDE(tmp);
    }

    /* if any part of the curve is a circular arc, the arc may extend
     * beyond the listed vertices
     */
    for (i = 0; i < skt->curve.count; i++) {
	struct carc_seg *csg=(struct carc_seg *)skt->curve.segment[i];
	struct carc_seg *csg_extr=(struct carc_seg *)extr->crv.segment[i];
	point_t center;

	if (csg->magic != CURVE_CARC_MAGIC)
	    continue;

	if (csg->radius <= 0.0) {
	    /* full circle */
	    point_t start;
	    fastf_t radius;

	    csg_extr->center = csg->end;
	    VJOIN2(start, eip->V, skt->verts[csg->start][0], eip->u_vec, skt->verts[csg->start][1], eip->v_vec);
	    VJOIN2(center, eip->V, skt->verts[csg->end][0], eip->u_vec, skt->verts[csg->end][1], eip->v_vec);
	    VSUB2(tmp, start, center);
	    radius = MAGNITUDE(tmp);
	    csg_extr->radius = -radius;	/* need the correct magnitude for normal calculation */

	    for (j=X; j<=Z; j++) {
		tmp_f = radius * ldir[j];
		VJOIN1(tmp, center, tmp_f, xyz[j]);
		VMINMAX(stp->st_min, stp->st_max, tmp);
		VADD2(tmp, tmp, eip->h);
		VMINMAX(stp->st_min, stp->st_max, tmp);

		VJOIN1(tmp, center, -tmp_f, xyz[j]);
		VMINMAX(stp->st_min, stp->st_max, tmp);
		VADD2(tmp, tmp, eip->h);
		VMINMAX(stp->st_min, stp->st_max, tmp);
	    }
	} else {
	    /* circular arc */
	    point_t start, end, mid;
	    vect_t s_to_m;
	    vect_t bisector;
	    fastf_t dist;
	    fastf_t magsq_s2m;

	    VJOIN2(start, eip->V, skt->verts[csg->start][0], eip->u_vec, skt->verts[csg->start][1], eip->v_vec);
	    VJOIN2(end, eip->V, skt->verts[csg->end][0], eip->u_vec, skt->verts[csg->end][1], eip->v_vec);
	    VBLEND2(mid, 0.5, start, 0.5, end);
	    VSUB2(s_to_m, mid, start);
	    VCROSS(bisector, extr->pl1, s_to_m);
	    VUNITIZE(bisector);
	    magsq_s2m = MAGSQ(s_to_m);
	    csg_extr->radius = csg->radius * extr->uv_scale;
	    if (magsq_s2m > csg_extr->radius*csg_extr->radius) {
		fastf_t max_radius;

		max_radius = sqrt(magsq_s2m);
		if (NEAR_EQUAL(max_radius, csg_extr->radius, RT_LEN_TOL)) {
		    csg_extr->radius = max_radius;
		} else {
		    bu_log("Impossible radius for circular arc in extrusion (%s), is %g, cannot be more than %g!\n",
			   stp->st_dp->d_namep, csg_extr->radius, sqrt(magsq_s2m));
		    bu_log("Difference is %g\n", max_radius - csg->radius);
		    return -1;
		}
	    }
	    dist = sqrt(csg_extr->radius*csg_extr->radius - magsq_s2m);

	    /* save arc center */
	    if (csg->center_is_left) {
		VJOIN1(center, mid, dist, bisector);
	    } else {
		VJOIN1(center, mid, -dist, bisector);
	    }
	    MAT4X3PNT(extr->verts[curr_vert], extr->rot, center);
	    csg_extr->center = curr_vert;
	    curr_vert++;

	    for (j=X; j<=Z; j++) {
		tmp_f = csg_extr->radius * ldir[j];
		VJOIN1(tmp, center, tmp_f, xyz[j]);
		VMINMAX(stp->st_min, stp->st_max, tmp);
		VADD2(tmp, tmp, eip->h);
		VMINMAX(stp->st_min, stp->st_max, tmp);

		VJOIN1(tmp, center, -tmp_f, xyz[j]);
		VMINMAX(stp->st_min, stp->st_max, tmp);
		VADD2(tmp, tmp, eip->h);
		VMINMAX(stp->st_min, stp->st_max, tmp);
	    }
	}
    }

    VBLEND2(stp->st_center, 0.5, stp->st_min, 0.5, stp->st_max);
    VSUB2(tmp, stp->st_max, stp->st_min);
    stp->st_aradius = 0.5 * MAGNITUDE(tmp);
    stp->st_bradius = stp->st_aradius;

    return 0;              /* OK */
}


void
rt_extrude_print(const struct soltab *stp)
{
    struct extrude_specific *extr=(struct extrude_specific *)stp->st_specific;

    VPRINT("u vector", extr->u_vec);
    VPRINT("v vector", extr->v_vec);
    VPRINT("h vector", extr->unit_h);
    bn_mat_print("rotation matrix", extr->rot);
    bn_mat_print("inverse rotation matrix", extr->irot);
}


static int
get_quadrant(fastf_t *v, fastf_t *local_x, fastf_t *local_y, fastf_t *vx, fastf_t *vy)
{

    *vx = V2DOT(v, local_x);
    *vy = V2DOT(v, local_y);

    if (*vy >= 0.0) {
	if (*vx >= 0.0)
	    return 1;
	else
	    return 2;
    } else {
	if (*vx >= 0.0)
	    return 4;
	else
	    return 3;
    }
}


/*
 * TODO: seems appropriate to make this into a proper libbn function
 */
static int
isect_line2_ellipse(vect2d_t dist, const vect2d_t ray_start, const vect2d_t ray_dir, const vect2d_t center, const vect2d_t ra, const vect2d_t rb)
{
    fastf_t a, b, c;
    point2d_t pmc;
    fastf_t pmcda, pmcdb;
    fastf_t ra_sq, rb_sq;
    fastf_t ra_4, rb_4;
    fastf_t dda, ddb;
    fastf_t disc;

    V2SUB2(pmc, ray_start, center);
    pmcda = V2DOT(pmc, ra);
    pmcdb = V2DOT(pmc, rb);
    ra_sq = V2DOT(ra, ra);
    ra_4 = ra_sq * ra_sq;
    rb_sq = V2DOT(rb, rb);
    rb_4 = rb_sq * rb_sq;
    if (ra_4 <= SMALL_FASTF || rb_4 <= SMALL_FASTF) {
	bu_log("ray (%g %g) -> (%g %g), semi-axes  = (%g %g) and (%g %g), center = (%g %g)\n",
	       V2ARGS(ray_start), V2ARGS(ray_dir), V2ARGS(ra), V2ARGS(rb), V2ARGS(center));
	bu_bomb("ERROR: isect_line2_ellipse: semi-axis length is too small!\n");
    }

    dda = V2DOT(ray_dir, ra);
    ddb = V2DOT(ray_dir, rb);

    a = dda*dda/ra_4 + ddb*ddb/rb_4;
    b = 2.0 * (pmcda*dda/ra_4 + pmcdb*ddb/rb_4);
    c = pmcda*pmcda/ra_4 + pmcdb*pmcdb/rb_4 - 1.0;

    disc = b*b - 4.0*a*c;
    if (disc < 0.0)
	return 0;

    if (disc <= SMALL_FASTF) {
	dist[0] = -b/(2.0*a);
	return 1;
    }

    dist[0] = (-b - sqrt(disc)) / (2.0*a);
    dist[1] = (-b + sqrt(disc)) / (2.0*a);
    return 2;
}


/*
 * TODO: seems appropriate to make this into a proper libbn function
 */
static int
isect_line_earc(vect2d_t dist, const vect_t ray_start, const vect_t ray_dir, const vect_t center, const vect_t ra, const vect_t rb, const vect_t norm, const vect_t start, const vect_t end, int orientation)


    /* 0 -> ccw, !0 -> cw */
{
    int i;
    int dist_count;
    vect_t local_x, local_y, local_z;
    fastf_t vx, vy;
    fastf_t ex, ey;
    fastf_t sx, sy;
    int quad_start, quad_end, quad_pt;
    point2d_t to_pt, pt;

    dist_count = isect_line2_ellipse(dist, ray_start, ray_dir, center, ra, rb);

    if (dist_count == 0)
	return 0;

    if (orientation) {
	VREVERSE(local_z, norm);
    } else {
	VMOVE(local_z, norm);
    }

    VMOVE(local_x, ra);

    VCROSS(local_y, local_z, local_x);

    V2SUB2(to_pt, end, center);
    quad_end = get_quadrant(to_pt, local_x, local_y, &ex, &ey);
    V2SUB2(to_pt, start, center);
    quad_start = get_quadrant(to_pt, local_x, local_y, &sx, &sy);

    i = 0;
    while (i < dist_count) {
	int omit;

	omit = 0;
	V2JOIN1(pt, ray_start, dist[i], ray_dir);
	V2SUB2(to_pt, pt, center);
	quad_pt = get_quadrant(to_pt, local_x, local_y, &vx, &vy);

	if (quad_start < quad_end) {
	    if (quad_pt > quad_end)
		omit = 1;
	    else if (quad_pt < quad_start)
		omit = 1;
	    else if (quad_pt == quad_end) {
		switch (quad_pt) {
		    case 1:
		    case 2:
			if (vx < ex)
			    omit = 1;
			break;
		    case 3:
		    case 4:
			if (vx > ex)
			    omit = 1;
			break;
		}
	    } else if (quad_pt == quad_start) {
		switch (quad_pt) {
		    case 1:
		    case 2:
			if (vx > sx)
			    omit = 1;
			break;
		    case 3:
		    case 4:
			if (vx < sx)
			    omit = 1;
			break;
		}
	    }
	} else if (quad_start > quad_end) {
	    if (quad_pt > quad_end && quad_pt < quad_start)
		omit = 1;
	    else if (quad_pt == quad_end) {
		switch (quad_pt) {
		    case 1:
		    case 2:
			if (vx < ex)
			    omit = 1;
			break;
		    case 3:
		    case 4:
			if (vx > ex)
			    omit = 1;
			break;
		}
	    } else if (quad_pt == quad_start) {
		switch (quad_pt) {
		    case 1:
		    case 2:
			if (vx > sx)
			    omit = 1;
			break;
		    case 3:
		    case 4:
			if (vx < sx)
			    omit = 1;
			break;
		}
	    }
	} else {
	    /* quad_start == quad_end */
	    if (quad_pt != quad_start)
		omit = 1;
	    else {
		switch (quad_pt) {
		    case 1:
		    case 2:
			if (vx < ex || vx > sx)
			    omit = 1;
			break;
		    case 3:
		    case 4:
			if (vx > ex || vx < sx)
			    omit = 1;
			break;
		}
	    }
	}
	if (omit) {
	    if (i == 0)
		dist[0] = dist[1];
	    dist_count--;
	} else {
	    i++;
	}
    }

    return dist_count;
}


/**
 * Intersect a ray with a extrude.  If an intersection occurs, a
 * struct seg will be acquired and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_extrude_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct extrude_specific *extr=(struct extrude_specific *)stp->st_specific;
    size_t i, j, k;
    long counter;
    fastf_t dist_top, dist_bottom, to_bottom = 0;
    vect2d_t dist = V2INIT_ZERO;
    fastf_t dot_pl1, dir_dot_z;
    point_t tmp, tmp2;
    point_t ray_start;			/* 2D */
    vect_t ray_dir, ray_dir_unit;	/* 2D */
    struct rt_curve *crv;
    struct hit hits[MAX_HITS] = {RT_HIT_INIT_ZERO};
    fastf_t dists_before[MAX_HITS] = {0.0};
    fastf_t dists_after[MAX_HITS] = {0.0};
    fastf_t *dists=NULL;
    size_t dist_count = 0;
    size_t hit_count = 0;
    size_t hits_before_bottom = 0, hits_after_top = 0;
    int code;
    int check_inout = 0;
    int top_face=TOP_FACE, bot_face=BOTTOM_FACE;
    int surfno= -42;
    int free_dists = 0;
    point2d_t *verts;
    point2d_t *intercept;
    point2d_t *normal = NULL;
    point2d_t ray_perp;
    vect_t ra = VINIT_ZERO; /* 2D */
    vect_t rb = VINIT_ZERO; /* 2D */

    crv = &extr->crv;

    /* intersect with top and bottom planes */
    dot_pl1 = VDOT(rp->r_dir, extr->pl1);
    if (ZERO(dot_pl1)) {
	/* ray is parallel to top and bottom faces */
	dist_bottom = DIST_PNT_PLANE(rp->r_pt, extr->pl1);
	dist_top = DIST_PNT_PLANE(rp->r_pt, extr->pl2);
	if (dist_bottom < 0.0 && dist_top < 0.0)
	    return 0;
	if (dist_bottom > 0.0 && dist_top > 0.0)
	    return 0;
	dist_bottom = -MAX_FASTF;
	dist_top = MAX_FASTF;
    } else {
	dist_bottom = -DIST_PNT_PLANE(rp->r_pt, extr->pl1)/dot_pl1;
	to_bottom = dist_bottom;				/* need to remember this */
	dist_top = -DIST_PNT_PLANE(rp->r_pt, extr->pl2)/dot_pl1;	/* pl1 and pl2 are parallel */
	if (dist_bottom > dist_top) {
	    fastf_t tmp1;

	    tmp1 = dist_bottom;
	    dist_bottom = dist_top;
	    dist_top = tmp1;
	    top_face = BOTTOM_FACE;
	    bot_face = TOP_FACE;
	}
    }

    /* rotate ray */
    MAT4X3PNT(ray_start, extr->rot, rp->r_pt);
    MAT4X3VEC(ray_dir, extr->rot, rp->r_dir);

    dir_dot_z = ray_dir[Z];
    if (dir_dot_z < 0.0)
	dir_dot_z = -dir_dot_z;

    /* Previously using SMALL_FASTF. A tolerance of dist_sq is
     * also too small (i.e. 1.0-16); using dist. There were cases
     * where dir_dot_z was very close to 1.0, but not close enough
     * to switch to using u vector as the ray direction and yet still
     * close enough to cause a miss when there should have been a hit.
     */
    if (NEAR_EQUAL(dir_dot_z, 1.0, extr_tol.dist)) {
	/* ray is parallel to extrusion vector set mode to just count
	 * intersections for Jordan Theorem
	 */
	check_inout = 1;

	/* set the ray start to the intersection of the original ray
	 * and the base plane
	 */
	VJOIN1(tmp, rp->r_pt, to_bottom, rp->r_dir);
	MAT4X3PNT(ray_start, extr->rot, tmp);

	/* use the u vector as the ray direction */
	VMOVE(ray_dir, extr->u_vec);
    }

    /* intersect with projected curve */
    for (i = 0; i < crv->count; i++) {
	uint32_t *lng=(uint32_t *)crv->segment[i];
	struct line_seg *lsg;
	struct carc_seg *csg=NULL;
	struct bezier_seg *bsg=NULL;
	fastf_t diff;

	free_dists = 0;
	switch (*lng) {
	    case CURVE_LSEG_MAGIC:
		lsg = (struct line_seg *)lng;
		VSUB2(tmp, extr->verts[lsg->end], extr->verts[lsg->start]);
		VMOVE(tmp2, extr->verts[lsg->start]);
		code = bg_isect_line2_line2(dist, ray_start, ray_dir, tmp2, tmp, &extr_tol);

		if (code < 1)
		    continue;

		if (dist[1] > 1.0 || dist[1] < 0.0)
		    continue;

		dists = dist;
		dist_count = 1;
		surfno = LINE_SEG;
		break;
	    case CURVE_CARC_MAGIC:
		/* circular arcs become elliptical arcs when projected
		 * in the XY-plane
		 */
		csg = (struct carc_seg *)lng;
		{
		    fastf_t radius;
		    point_t center = VINIT_ZERO;

		    if (csg->radius <= 0.0) {
			/* full circle */
			radius = -csg->radius;

			/* build the ellipse, this actually builds a
			 * circle in 3D, but the intersection routine
			 * only uses the X and Y components
			 */
			VSCALE(ra, extr->rot_axis, radius);
			VSCALE(rb, extr->perp, radius);

			VSET(center, extr->verts[csg->end][X], extr->verts[csg->end][Y], 0.0);
			dist_count = isect_line2_ellipse(dist, ray_start, ray_dir, center, ra, rb);
			MAT4X3PNT(tmp, extr->irot, extr->verts[csg->end]); /* used later in hit->vpriv */
		    } else {
			VSCALE(ra, extr->rot_axis, csg->radius);
			VSCALE(rb, extr->perp, csg->radius);
			VSET(center, extr->verts[csg->center][X], extr->verts[csg->center][Y], 0.0);
			dist_count = isect_line_earc(dist, ray_start, ray_dir, center, ra, rb, extr->pl1_rot, extr->verts[csg->start], extr->verts[csg->end], csg->orientation);
			MAT4X3PNT(tmp, extr->irot, extr->verts[csg->center]); /* used later in hit->vpriv */
		    }
		}
		if (dist_count < 1)
		    continue;

		dists = dist;
		surfno = CARC_SEG;
		break;

	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		verts = (point2d_t *)bu_calloc(bsg->degree + 1, sizeof(point2d_t), "Bezier verts");
		for (j = 0; j <= (size_t)bsg->degree; j++) {
		    V2MOVE(verts[j], extr->verts[bsg->ctl_points[j]]);
		}

		V2MOVE(ray_dir_unit, ray_dir);
		diff = sqrt(MAG2SQ(ray_dir));

		ray_dir_unit[X] /= diff;
		ray_dir_unit[Y] /= diff;
		ray_dir_unit[Z] = 0.0;
		ray_perp[X] = ray_dir[Y];
		ray_perp[Y] = -ray_dir[X];

		dist_count = bezier_roots(verts, bsg->degree, &intercept, &normal, ray_start, ray_dir_unit, ray_perp, 0, extr_tol.dist);
		if (dist_count) {
		    free_dists = 1;
		    dists = (fastf_t *)bu_calloc(dist_count, sizeof(fastf_t), "dists (Bezier)");
		    for (j = 0; j < dist_count; j++) {
			point2d_t to_pt;
			V2SUB2(to_pt, intercept[j], ray_start);
			dists[j] = V2DOT(to_pt, ray_dir_unit) / diff;
		    }
		    bu_free((char *)intercept, "Bezier intercept");
		    surfno = BEZIER_SEG;
		}

		bu_free((char *)verts, "Bezier verts");
		break;

	    case CURVE_NURB_MAGIC:
		break;
	    default:
		bu_log("Unrecognized segment type in sketch referenced by extrusion (%s)\n",stp->st_dp->d_namep);
		bu_bomb("Unrecognized segment type in sketch\n");
		break;
	}

	/* eliminate duplicate hit distances */
	for (j = 0; j < hit_count; j++) {
	    k = 0;
	    while (k < dist_count) {
		diff = dists[k] - hits[j].hit_dist;
		if (NEAR_ZERO(diff, extr_tol.dist)) {
		    size_t n;
		    for (n=k; n<dist_count-1; n++) {
			dists[n] = dists[n+1];
			if (*lng == CURVE_BEZIER_MAGIC) {
			    V2MOVE(normal[n], normal[n+1]);
			}
		    }
		    dist_count--;
		} else {
		    k++;
		}
	    }
	}

	/* eliminate duplicate hits below the bottom plane of the
	 * extrusion
	 */
	for (j = 0; j < hits_before_bottom; j++) {
	    k = 0;
	    while (k < dist_count) {
		diff = dists[k] - dists_before[j];
		if (NEAR_ZERO(diff, extr_tol.dist)) {
		    size_t n;
		    for (n=k; n<dist_count-1; n++) {
			dists[n] = dists[n+1];
			if (*lng == CURVE_BEZIER_MAGIC) {
			    V2MOVE(normal[n], normal[n+1]);
			}
		    }
		    dist_count--;
		} else {
		    k++;
		}
	    }
	}

	/* eliminate duplicate hits above the top plane of the
	 * extrusion
	 */
	for (j = 0; j < hits_after_top; j++) {
	    k = 0;
	    while (k < dist_count) {
		diff = dists[k] - dists_after[j];
		if (NEAR_ZERO(diff, extr_tol.dist)) {
		    size_t n;
		    for (n=k; n<dist_count-1; n++)
			dists[n] = dists[n+1];
		    dist_count--;
		} else {
		    k++;
		}
	    }
	}

	/* if we are just doing the Jordan curve theorem */
	if (check_inout) {
	    for (j = 0; j < dist_count; j++) {
		if (dists[j] < 0.0)
		    hit_count++;
	    }
	    continue;
	}

	/* process remaining distances into hits */
	for (j = 0; j < dist_count; j++) {
	    if (dists[j] < dist_bottom) {
		if (hits_before_bottom >= MAX_HITS) {
		    bu_log("ERROR: rt_extrude_shot: too many hits before bottom on extrusion (%s), limit is %d\n",
			   stp->st_dp->d_namep, MAX_HITS);
		    bu_bomb("ERROR: rt_extrude_shot: too many hits before bottom on extrusion\n");
		}
		dists_before[hits_before_bottom] = dists[j];
		hits_before_bottom++;
		continue;
	    }
	    if (dists[j] > dist_top) {
		if (hits_after_top >= MAX_HITS) {
		    bu_log("ERROR: rt_extrude_shot: too many hits after top on extrusion (%s), limit is %d\n",
			   stp->st_dp->d_namep, MAX_HITS);
		    bu_bomb("ERROR: rt_extrude_shot: too many hits after top on extrusion\n");
		}
		dists_after[hits_after_top] = dists[j];
		hits_after_top++;

		continue;
	    }

	    /* got a hit at distance dists[j] */
	    if (hit_count >= MAX_HITS) {
		bu_log("Too many hits on extrusion (%s), limit is %d\n",
		       stp->st_dp->d_namep, MAX_HITS);
		bu_bomb("Too many hits on extrusion\n");
	    }
	    hits[hit_count].hit_magic = RT_HIT_MAGIC;
	    hits[hit_count].hit_dist = dists[j];
	    hits[hit_count].hit_surfno = surfno;
	    switch (*lng) {
		case CURVE_CARC_MAGIC:
		    hits[hit_count].hit_private = (void *)csg;
		    VMOVE(hits[hit_count].hit_vpriv, tmp);
		    break;
		case CURVE_LSEG_MAGIC:
		    VMOVE(hits[hit_count].hit_vpriv, tmp);
		    break;
		case CURVE_BEZIER_MAGIC:
		    V2MOVE(hits[hit_count].hit_vpriv, normal[j]);
		    hits[hit_count].hit_vpriv[Z] = 0.0;
		    break;
		default:
		    bu_log("ERROR: rt_extrude_shot: unrecognized segment type in solid %s\n",
			   stp->st_dp->d_namep);
		    bu_bomb("ERROR: rt_extrude_shot: unrecognized segment type in solid\n");
		    break;
	    }
	    hit_count++;
	}
	if (free_dists)
	    bu_free((char *)dists, "dists");
    }

    if (check_inout) {
	if (hit_count&1) {
	    struct seg *segp;

	    hits[0].hit_magic = RT_HIT_MAGIC;
	    hits[0].hit_dist = dist_bottom;
	    hits[0].hit_surfno = bot_face;
	    VMOVE(hits[0].hit_normal, extr->pl1);

	    hits[1].hit_magic = RT_HIT_MAGIC;
	    hits[1].hit_dist = dist_top;
	    hits[1].hit_surfno = -top_face;
	    VMOVE(hits[1].hit_normal, extr->pl1);

	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;
	    segp->seg_in = hits[0];	/* struct copy */
	    segp->seg_out = hits[1];	/* struct copy */
	    BU_LIST_INSERT(&(seghead->l), &(segp->l));

	    return 2;
	} else {
	    return 0;
	}
    }

    if (hit_count) {
	/* Sort hits, Near to Far */
	primitive_hitsort(hits, hit_count);
    }

    if (hits_before_bottom & 1) {
	if (hit_count >= MAX_HITS) {
	    bu_log("Too many hits on extrusion (%s), limit is %d\n",
		   stp->st_dp->d_namep, MAX_HITS);
	    bu_bomb("Too many hits on extrusion\n");
	}
	for (counter=hit_count-1; counter >= 0; counter--)
	    hits[counter+1] = hits[counter];
	hits[0].hit_magic = RT_HIT_MAGIC;
	hits[0].hit_dist = dist_bottom;
	hits[0].hit_surfno = bot_face;
	VMOVE(hits[0].hit_normal, extr->pl1);
	hit_count++;
    }

    if (hits_after_top & 1) {
	if (hit_count >= MAX_HITS) {
	    bu_log("Too many hits on extrusion (%s), limit is %d\n",
		   stp->st_dp->d_namep, MAX_HITS);
	    bu_bomb("Too many hits on extrusion\n");
	}
	hits[hit_count].hit_magic = RT_HIT_MAGIC;
	hits[hit_count].hit_dist = dist_top;
	hits[hit_count].hit_surfno = top_face;
	VMOVE(hits[hit_count].hit_normal, extr->pl1);
	hit_count++;
    }

    if (hit_count%2) {
	point_t pt;

	if (hit_count != 1) {
	    bu_log("ERROR: rt_extrude_shot(): odd number of hits (%zu) (ignoring last hit)\n", hit_count);
	    bu_log("ray start = (%20.10f %20.10f %20.10f)\n", V3ARGS(rp->r_pt));
	    bu_log("\tray dir = (%20.10f %20.10f %20.10f)", V3ARGS(rp->r_dir));
	    VJOIN1(pt, rp->r_pt, hits[hit_count-1].hit_dist, rp->r_dir);
	    bu_log("\tignored hit at (%g %g %g)\n", V3ARGS(pt));
	}
	hit_count--;
    }

    /* build segments */
    {
	size_t cnt;
	struct seg *segp;

	for (cnt = 0; cnt < hit_count; cnt += 2) {
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;
	    segp->seg_in = hits[cnt];	/* struct copy */
	    segp->seg_out = hits[cnt+1];	/* struct copy */
	    segp->seg_out.hit_surfno = -segp->seg_out.hit_surfno;	/* for exit hits */
	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	}
    }

    return hit_count;
}


/**
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_extrude_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct extrude_specific *extr=(struct extrude_specific *)stp->st_specific;
    fastf_t alpha;
    point_t hit_in_plane;
    vect_t tmp, tmp2;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);

    switch (hitp->hit_surfno) {
	case LINE_SEG:
	    MAT4X3VEC(tmp, extr->irot, hitp->hit_vpriv);
	    VCROSS(hitp->hit_normal, extr->unit_h, tmp);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case -LINE_SEG:
	    MAT4X3VEC(tmp, extr->irot, hitp->hit_vpriv);
	    VCROSS(hitp->hit_normal, extr->unit_h, tmp);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case TOP_FACE:
	case BOTTOM_FACE:
	case -TOP_FACE:
	case -BOTTOM_FACE:
	    break;
	case CARC_SEG:
	case -CARC_SEG:
	    alpha = DIST_PNT_PLANE(hitp->hit_point, extr->pl1) / VDOT(extr->unit_h, extr->pl1);
	    VJOIN1(hit_in_plane, hitp->hit_point, -alpha, extr->unit_h);
	    VSUB2(tmp, hit_in_plane, hitp->hit_vpriv);
	    VCROSS(tmp2, extr->pl1, tmp);
	    VCROSS(hitp->hit_normal, tmp2, extr->unit_h);
	    VUNITIZE(hitp->hit_normal);
	    break;
	case BEZIER_SEG:
	case -BEZIER_SEG:
	    MAT4X3VEC(hitp->hit_normal, extr->irot, hitp->hit_vpriv);
	    VUNITIZE(hitp->hit_normal);
	    break;
	default:
	    bu_bomb("ERROR: rt_extrude_norm(): unrecognized surf_no in hit structure!\n");
	    break;
    }
    if (hitp->hit_surfno < 0) {
	if (VDOT(hitp->hit_normal, rp->r_dir) < 0.0)
	    VREVERSE(hitp->hit_normal, hitp->hit_normal);
    } else {
	if (VDOT(hitp->hit_normal, rp->r_dir) > 0.0)
	    VREVERSE(hitp->hit_normal, hitp->hit_normal);
    }

}


/**
 * Return the curvature of the extrude.
 */
void
rt_extrude_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    struct extrude_specific *extr=(struct extrude_specific *)stp->st_specific;
    struct carc_seg *csg = NULL;
    fastf_t radius, a, b, a_sq, b_sq;
    fastf_t curvature, tmp, dota, dotb;
    fastf_t der = 0.0;
    vect_t diff = VINIT_ZERO;
    vect_t ra = VINIT_ZERO;
    vect_t rb = VINIT_ZERO;

    switch (hitp->hit_surfno) {
	case LINE_SEG:
	case -LINE_SEG:
	    VMOVE(cvp->crv_pdir, hitp->hit_vpriv);
	    VUNITIZE(cvp->crv_pdir);
	    cvp->crv_c1 = cvp->crv_c2 = 0;
	    break;
	case CARC_SEG:
	case -CARC_SEG:
	    /* curvature for an ellipse (the rotated and projected
	     * circular arc) in XY-plane based on curvature for
	     * ellipse = |ra||rb|/(|derivative|**3)
	     */
	    csg = (struct carc_seg *)hitp->hit_private;
	    VCROSS(cvp->crv_pdir, extr->unit_h, hitp->hit_normal);
	    VSUB2(diff, hitp->hit_point, hitp->hit_vpriv);
	    if (csg->radius < 0.0)
		radius = -csg->radius;
	    else
		radius = csg->radius;
	    VSCALE(ra, extr->rot_axis, radius);
	    VSCALE(rb, extr->perp, radius);

	    a_sq = MAG2SQ(ra);
	    b_sq = MAG2SQ(rb);
	    a = sqrt(a_sq);
	    b = sqrt(b_sq);
	    dota = VDOT(diff, ra);
	    dotb = VDOT(diff, rb);
	    tmp = (a_sq/(b_sq*b_sq))*dotb*dotb + (b_sq/(a_sq*a_sq))*dota*dota;
	    der = sqrt(tmp);
	    curvature = a*b/(der*der*der);
	    if (VDOT(hitp->hit_normal, diff) > 0.0)
		cvp->crv_c1 = curvature;
	    else
		cvp->crv_c1 = -curvature;
	    cvp->crv_c2 = 0;
	    break;
    }
}

void
rt_extrude_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);

    uvp->uv_u = hitp->hit_vpriv[X];
    uvp->uv_v = hitp->hit_vpriv[Y];
    uvp->uv_du = 0;
    uvp->uv_dv = 0;
}

void
rt_extrude_free(struct soltab *stp)
{
    struct extrude_specific *extrude =
	(struct extrude_specific *)stp->st_specific;

    if (extrude->verts)
	bu_free((char *)extrude->verts, "extrude->verts");
    rt_curve_free(&(extrude->crv));
    BU_PUT(extrude, struct extrude_specific);
}


int
rt_extrude_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    struct rt_extrude_internal *extrude_ip;
    struct rt_curve *crv = NULL;
    struct rt_sketch_internal *sketch_ip;
    point_t end_of_h;
    size_t i1, i2, nused1, nused2;
    struct bv_vlist *vp1, *vp2, *vp2_start;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    RT_VLFREE_INIT();
    struct bu_list *vlfree = &rt_vlfree;
    extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extrude_ip);

    if (!extrude_ip->skt) {
	bu_log("ERROR: no sketch to extrude!\n");
	BV_ADD_VLIST(vlfree, vhead, extrude_ip->V, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, extrude_ip->V, BV_VLIST_LINE_DRAW);
	return 0;
    }

    sketch_ip = extrude_ip->skt;
    RT_SKETCH_CK_MAGIC(sketch_ip);

    crv = &sketch_ip->curve;

    /* empty sketch, nothing to do */
    if (crv->count == 0) {
	if (extrude_ip->sketch_name) {
	    bu_log("Sketch [%s] is empty, nothing to draw\n", extrude_ip->sketch_name);
	} else {
	    bu_log("Unnamed sketch is empty, nothing to draw\n");
	}
	BV_ADD_VLIST(vlfree, vhead, extrude_ip->V, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, extrude_ip->V, BV_VLIST_LINE_DRAW);
	return 0;
    }

    /* plot bottom curve */
    vp1 = BU_LIST_LAST(bv_vlist, vhead);
    nused1 = vp1->nused;
    if (curve_to_vlist(vlfree, vhead, ttol, extrude_ip->V, extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, crv)) {
	bu_log("ERROR: sketch (%s) references non-existent vertices!\n",
	       extrude_ip->sketch_name);
	return -1;
    }

    /* plot top curve */
    VADD2(end_of_h, extrude_ip->V, extrude_ip->h);
    vp2 = BU_LIST_LAST(bv_vlist, vhead);
    nused2 = vp2->nused;
    curve_to_vlist(vlfree, vhead, ttol, end_of_h, extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, crv);

    /* plot connecting lines */
    vp2_start = vp2;
    i1 = nused1;
    if (i1 >= vp1->nused) {
	i1 = 0;
	vp1 = BU_LIST_NEXT(bv_vlist, &vp1->l);
    }
    i2 = nused2;
    if (i2 >= vp2->nused) {
	i2 = 0;
	vp2 = BU_LIST_NEXT(bv_vlist, &vp2->l);
	nused2--;
    }

    while (vp1 != vp2_start || (i1 < BV_VLIST_CHUNK && i2 < BV_VLIST_CHUNK && i1 != nused2)) {
	BV_ADD_VLIST(vlfree, vhead, vp1->pt[i1], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, vp2->pt[i2], BV_VLIST_LINE_DRAW);
	i1++;
	if (i1 >= vp1->nused) {
	    i1 = 0;
	    vp1 = BU_LIST_NEXT(bv_vlist, &vp1->l);
	}
	i2++;
	if (i2 >= vp2->nused) {
	    i2 = 0;
	    vp2 = BU_LIST_NEXT(bv_vlist, &vp2->l);
	}
    }

    return 0;
}

void
rt_extrude_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    struct rt_extrude_internal *eip;
    struct rt_sketch_internal *skt;
    struct rt_db_internal db_skt;
    point_t skt_cent;
    point_t middle_h;
    eip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(eip);
    skt = eip->skt;
    RT_SKETCH_CK_MAGIC(skt);

    RT_DB_INTERNAL_INIT(&db_skt);
    db_skt.idb_ptr = (void *)skt;

    rt_sketch_centroid(&skt_cent, &db_skt);

    VSCALE(middle_h, eip->h, 0.5);
    VADD2(*cent, skt_cent, middle_h);
}


void
get_indices(void *seg, int *start, int *end)
{
    struct carc_seg *csg;
    struct nurb_seg *nsg;
    struct bezier_seg *bsg;
    struct line_seg *lsg=(struct line_seg *)seg;

    switch (lsg->magic) {
	case CURVE_LSEG_MAGIC:
	    *start = lsg->start;
	    *end = lsg->end;
	    break;
	case CURVE_CARC_MAGIC:
	    csg = (struct carc_seg *)seg;
	    if (csg->radius < 0.0) {
		*start = csg->start;
		*end = *start;
		break;
	    }
	    *start = csg->start;
	    *end = csg->end;
	    break;
	case CURVE_NURB_MAGIC:
	    nsg = (struct nurb_seg *)seg;
	    *start = nsg->ctl_points[0];
	    *end = nsg->ctl_points[nsg->c_size-1];
	    break;
	case CURVE_BEZIER_MAGIC:
	    bsg = (struct bezier_seg *)seg;
	    *start = bsg->ctl_points[0];
	    *end = bsg->ctl_points[bsg->degree];
	    break;
    }
}


static int
get_seg_midpoint(void *seg, struct rt_sketch_internal *skt, point2d_t pt)
{
    struct edge_g_cnurb eg;
    point_t tmp_pt;
    struct line_seg *lsg;
    struct carc_seg *csg;
    struct nurb_seg *nsg;
    struct bezier_seg *bsg;
    uint32_t *lng;
    point2d_t *V;
    point2d_t pta = V2INIT_ZERO;
    int i;
    int coords;

    lng = (uint32_t *)seg;

    switch (*lng) {
	case CURVE_LSEG_MAGIC:
	    lsg = (struct line_seg *)lng;
	    V2ADD2(pta, skt->verts[lsg->start], skt->verts[lsg->end]);
	    V2SCALE(pt, pta, 0.5);
	    break;
	case CURVE_CARC_MAGIC:
	    csg = (struct carc_seg *)lng;
	    if (csg->radius < 0.0) {
		V2MOVE(pt, skt->verts[csg->start]);
	    } else {
		point2d_t start2d = V2INIT_ZERO;
		point2d_t end2d = V2INIT_ZERO;
		point2d_t mid_pt = V2INIT_ZERO;
		point2d_t s2m = V2INIT_ZERO;
		point2d_t dir = V2INIT_ZERO;
		point2d_t center2d = V2INIT_ZERO;
		fastf_t tmp_len, len_sq, mid_ang, s2m_len_sq, cross_z;
		fastf_t start_ang, end_ang;

		/* this is an arc (not a full circle) */
		V2MOVE(start2d, skt->verts[csg->start]);
		V2MOVE(end2d, skt->verts[csg->end]);
		mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
		mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
		V2SUB2(s2m, mid_pt, start2d);
		dir[0] = -s2m[1];
		dir[1] = s2m[0];
		s2m_len_sq =  s2m[0]*s2m[0] + s2m[1]*s2m[1];
		if (s2m_len_sq < SMALL_FASTF) {
		    bu_log("start and end points are too close together in circular arc of sketch\n");
		    s2m_len_sq = SMALL_FASTF;
		}
		len_sq = csg->radius*csg->radius - s2m_len_sq;
		if (len_sq < 0.0) {
		    bu_log("Impossible radius for specified start and end points in circular arc\n");
		    len_sq = 0;
		}
		tmp_len = sqrt(dir[0]*dir[0] + dir[1]*dir[1]);
		dir[0] = dir[0] / tmp_len;
		dir[1] = dir[1] / tmp_len;
		tmp_len = sqrt(len_sq);
		V2JOIN1(center2d, mid_pt, tmp_len, dir);

		/* check center location */
		cross_z = (end2d[X] - start2d[X])*(center2d[Y] - start2d[Y]) -
		    (end2d[Y] - start2d[Y])*(center2d[X] - start2d[X]);
		if (!(cross_z > 0.0 && csg->center_is_left)) {
		    V2JOIN1(center2d, mid_pt, -tmp_len, dir);
		}
		start_ang = atan2(start2d[Y]-center2d[Y], start2d[X]-center2d[X]);
		end_ang = atan2(end2d[Y]-center2d[Y], end2d[X]-center2d[X]);
		if (csg->orientation) {
		    /* clock-wise */
		    while (end_ang > start_ang)
			end_ang -= M_2PI;
		} else {
		    /* counter-clock-wise */
		    while (end_ang < start_ang)
			end_ang += M_2PI;
		}

		/* get mid angle */
		mid_ang = (start_ang + end_ang) * 0.5;

		/* calculate mid point */
		pt[X] = center2d[X] + csg->radius * cos(mid_ang);
		pt[Y] = center2d[Y] + csg->radius * sin(mid_ang);
		break;
	    }
	    break;
	case CURVE_NURB_MAGIC:
	    nsg = (struct nurb_seg *)lng;

	    eg.l.magic = NMG_EDGE_G_CNURB_MAGIC;
	    eg.order = nsg->order;
	    eg.k.k_size = nsg->k.k_size;
	    eg.k.knots = nsg->k.knots;
	    eg.c_size = nsg->c_size;
	    coords = 2 + RT_NURB_IS_PT_RATIONAL(nsg->pt_type);
	    eg.pt_type = RT_NURB_MAKE_PT_TYPE(coords, 2, RT_NURB_IS_PT_RATIONAL(nsg->pt_type));
	    eg.ctl_points = (fastf_t *)bu_malloc(nsg->c_size * coords * sizeof(fastf_t), "eg.ctl_points");
	    if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
		for (i = 0; i < nsg->c_size; i++) {
		    V2MOVE(&eg.ctl_points[i*coords], skt->verts[nsg->ctl_points[i]]);
		    eg.ctl_points[(i+1)*coords - 1] = nsg->weights[i];
		}
	    } else {
		for (i = 0; i < nsg->c_size; i++) {
		    V2MOVE(&eg.ctl_points[i*coords], skt->verts[nsg->ctl_points[i]]);
		}
	    }
	    nmg_nurb_c_eval(&eg, (nsg->k.knots[nsg->k.k_size-1] - nsg->k.knots[0]) * 0.5, tmp_pt);
	    if (RT_NURB_IS_PT_RATIONAL(nsg->pt_type)) {
		int j;

		for (j = 0; j < coords-1; j++)
		    pt[j] = tmp_pt[j] / tmp_pt[coords-1];
	    } else {
		V2MOVE(pt, tmp_pt);
	    }
	    bu_free((char *)eg.ctl_points, "eg.ctl_points");
	    break;
	case CURVE_BEZIER_MAGIC:
	    bsg = (struct bezier_seg *)lng;
	    V = (point2d_t *)bu_calloc(bsg->degree+1, sizeof(point2d_t), "Bezier control points");
	    for (i = 0; i <= bsg->degree; i++) {
		V2MOVE(V[i], skt->verts[bsg->ctl_points[i]]);
	    }
	    bezier(V, bsg->degree, 0.51, NULL, NULL, pt, NULL);
	    bu_free((char *)V, "Bezier control points");
	    break;
	default:
	    bu_bomb("Unrecognized segment type in sketch\n");
	    break;
    }

    return 0;
}


struct loop_inter {
    int which_loop;
    int vert_index;	/* index of vertex intersected, or -1 if no hit on a vertex */
    fastf_t dist;	/* hit distance */
    struct loop_inter *next;
};


static void
isect_2D_loop_ray(point2d_t pta, point2d_t dir, struct bu_ptbl *loop, struct loop_inter **root,
		  int which_loop, struct rt_sketch_internal *ip, struct bn_tol *tol)
{
    size_t i, j;
    int code;
    vect_t norm;
    fastf_t dist[2];

    vect_t s2m, tmp_dir;
    fastf_t s2m_len_sq, len_sq, tmp_len, cross_z;

    vect_t ra = VINIT_ZERO;
    vect_t rb = VINIT_ZERO;
    point2d_t start2d = V2INIT_ZERO;
    point2d_t end2d = V2INIT_ZERO;
    point2d_t mid_pt = V2INIT_ZERO;
    point2d_t center2d = V2INIT_ZERO;

    norm[X] = -dir[Y];
    norm[Y] = dir[X];
    norm[Z] = 0.0;

    for (i = 0; i < BU_PTBL_LEN(loop); i++) {
	uint32_t *lng;
	struct loop_inter *inter;
	struct line_seg *lsg=NULL;
	struct carc_seg *csg=NULL;
	struct bezier_seg *bsg=NULL;
	point2d_t diff;
	fastf_t radius;
	point2d_t *verts;
	point2d_t *intercept;
	point2d_t *normal = NULL;

	lng = (uint32_t *)BU_PTBL_GET(loop, i);
	switch (*lng) {
	    case CURVE_LSEG_MAGIC: {
		point_t v3p = VINIT_ZERO;
		vect_t v3d = VINIT_ZERO;
		point_t d1 = VINIT_ZERO;
		V2MOVE(v3p, pta);
		V2MOVE(v3d, dir);

		lsg = (struct line_seg *)lng;
		V2SUB2(d1, ip->verts[lsg->end], ip->verts[lsg->start]);
		code = bg_isect_line2_lseg2(dist, v3p, v3d, ip->verts[lsg->start], d1, tol);
		if (code < 0)
		    break;
		if (code == 0) {
		    /* edge is collinear with ray */
		    /* add two intersections, one at each end vertex */
		    BU_ALLOC(inter, struct loop_inter);

		    inter->which_loop = which_loop;
		    inter->vert_index = lsg->start;
		    inter->dist = dist[0];
		    inter->next = NULL;
		    if (!(*root)) {
			(*root) = inter;
		    } else {
			inter->next = (*root);
			(*root) = inter;
		    }
		    BU_ALLOC(inter, struct loop_inter);

		    inter->which_loop = which_loop;
		    inter->vert_index = lsg->end;
		    inter->dist = dist[1];
		    inter->next = NULL;
		    inter->next = (*root);
		    (*root) = inter;
		} else if (code == 1) {
		    /* hit at start vertex */
		    BU_ALLOC(inter, struct loop_inter);

		    inter->which_loop = which_loop;
		    inter->vert_index = lsg->start;
		    inter->dist = dist[0];
		    inter->next = NULL;
		    if (!(*root)) {
			(*root) = inter;
		    } else {
			inter->next = (*root);
			(*root) = inter;
		    }
		} else if (code == 2) {
		    /* hit at end vertex */
		    BU_ALLOC(inter, struct loop_inter);

		    inter->which_loop = which_loop;
		    inter->vert_index = lsg->end;
		    inter->dist = dist[0];
		    inter->next = NULL;
		    if (!(*root)) {
			(*root) = inter;
		    } else {
			inter->next = (*root);
			(*root) = inter;
		    }
		} else {
		    /* hit on edge, not at a vertex */
		    BU_ALLOC(inter, struct loop_inter);

		    inter->which_loop = which_loop;
		    inter->vert_index = -1;
		    inter->dist = dist[0];
		    inter->next = NULL;
		    if (!(*root)) {
			(*root) = inter;
		    } else {
			inter->next = (*root);
			(*root) = inter;
		    }
		}
		break;
	    }
	    case CURVE_CARC_MAGIC:
		csg = (struct carc_seg *)lng;
		radius = csg->radius;
		if (csg->radius <= 0.0) {
		    V2SUB2(diff, ip->verts[csg->start], ip->verts[csg->end]);
		    radius = sqrt(MAG2SQ(diff));
		    ra[X] = radius;
		    ra[Y] = 0.0;
		    ra[Z] = 0.0;
		    rb[X] = 0.0;
		    rb[Y] = radius;
		    rb[Z] = 0.0;
		    code = isect_line2_ellipse(dist, pta, dir, ip->verts[csg->end],
					       ra, rb);

		    if (code <= 0)
			break;
		    for (j = 0; j < (size_t)code; j++) {
			BU_ALLOC(inter, struct loop_inter);

			inter->which_loop = which_loop;
			inter->vert_index = -1;
			inter->dist = dist[j];
			inter->next = NULL;
			if (!(*root)) {
			    (*root) = inter;
			} else {
			    inter->next = (*root);
			    (*root) = inter;
			}
		    }

		} else {
		    point_t v3p = VINIT_ZERO;
		    vect_t v3d = VINIT_ZERO;
		    V2MOVE(v3p, pta);
		    V2MOVE(v3d, dir);

		    point_t center = VINIT_ZERO;

		    V2MOVE(start2d, ip->verts[csg->start]);
		    V2MOVE(end2d, ip->verts[csg->end]);
		    mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
		    mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
		    V2SUB2(s2m, mid_pt, start2d);
		    tmp_dir[0] = -s2m[1];
		    tmp_dir[1] = s2m[0];
		    s2m_len_sq =  s2m[0]*s2m[0] + s2m[1]*s2m[1];
		    if (s2m_len_sq <= SMALL_FASTF) {
			bu_log("start and end points are too close together in circular arc of sketch\n");
			break;
		    }
		    len_sq = radius*radius - s2m_len_sq;
		    if (len_sq < 0.0) {
			bu_log("Impossible radius for specified start and end points in circular arc\n");
			break;
		    }
		    tmp_len = sqrt(tmp_dir[0]*tmp_dir[0] + tmp_dir[1]*tmp_dir[1]);
		    tmp_dir[0] = tmp_dir[0] / tmp_len;
		    tmp_dir[1] = tmp_dir[1] / tmp_len;
		    tmp_len = sqrt(len_sq);
		    V2JOIN1(center2d, mid_pt, tmp_len, tmp_dir);

		    /* check center location */
		    cross_z = (end2d[X] - start2d[X])*(center2d[Y] - start2d[Y]) - (end2d[Y] - start2d[Y])*(center2d[X] - start2d[X]);
		    if (!(cross_z > 0.0 && csg->center_is_left))
			V2JOIN1(center2d, mid_pt, -tmp_len, tmp_dir);

		    VSET(ra, radius, 0.0, 0.0);
		    VSET(rb, 0.0, radius, 0.0);
		    VSET(center, center2d[X], center2d[Y], 0.0);

		    code = isect_line_earc(dist, v3p, v3d, center, ra, rb,
					   norm, ip->verts[csg->start], ip->verts[csg->end], csg->orientation);
		    if (code <= 0)
			break;
		    for (j = 0; j < (size_t)code; j++) {
			BU_ALLOC(inter, struct loop_inter);

			inter->which_loop = which_loop;
			inter->vert_index = -1;
			inter->dist = dist[j];
			inter->next = NULL;
			if (!(*root)) {
			    (*root) = inter;
			} else {
			    inter->next = (*root);
			    (*root) = inter;
			}
		    }
		}
		break;

	    case CURVE_BEZIER_MAGIC:
		bsg = (struct bezier_seg *)lng;
		intercept = NULL;
		verts = (point2d_t *)bu_calloc(bsg->degree + 1, sizeof(point2d_t), "Bezier verts");
		for (j = 0; j <= (size_t)bsg->degree; j++) {
		    V2MOVE(verts[j], ip->verts[bsg->ctl_points[j]]);
		}

		code = bezier_roots(verts, bsg->degree, &intercept, &normal, pta, dir, norm, 0, tol->dist);
		for (j = 0; j < (size_t)code; j++) {
		    V2SUB2(diff, intercept[j], pta);
		    dist[0] = sqrt(MAG2SQ(diff));
		    BU_ALLOC(inter, struct loop_inter);

		    inter->which_loop = which_loop;
		    inter->vert_index = -1;
		    inter->dist = dist[0];
		    inter->next = NULL;
		    if (!(*root)) {
			(*root) = inter;
		    } else {
			inter->next = (*root);
			(*root) = inter;
		    }
		}

		if ((*intercept))
		    bu_free((char *)intercept, "Bezier Intercepts");
		if ((*normal))
		    bu_free((char *)normal, "Bezier normals");
		bu_free((char *)verts, "Bezier Ctl points");
		break;

	    default:
		bu_log("isect_2D_loop_ray: Unrecognized curve segment type x%x\n", *lng);
		bu_bomb("isect_2D_loop_ray: Unrecognized curve segment type\n");
		break;
	}
    }
}


static void
sort_intersections(struct loop_inter **root, struct bn_tol *tol)
{
    struct loop_inter *ptr, *prev, *pprev;
    int done = 0;
    fastf_t diff;

    /* eliminate any duplicates */
    ptr = (*root);
    while (ptr->next) {
	prev = ptr;
	ptr = ptr->next;
	if (ptr->vert_index > -1 && ptr->vert_index == prev->vert_index) {
	    prev->next = ptr->next;
	    bu_free((char *)ptr, "struct loop_inter");
	    ptr = prev;
	}
    }

    ptr = (*root);
    while (ptr->next) {
	prev = ptr;
	ptr = ptr->next;
	diff = fabs(ptr->dist - prev->dist);
	if (diff < tol->dist) {
	    prev->next = ptr->next;
	    bu_free((char *)ptr, "struct loop_inter");
	    ptr = prev;
	}
    }

    while (!done) {
	done = 1;
	ptr = (*root);
	prev = NULL;
	pprev = NULL;
	while (ptr->next) {
	    pprev = prev;
	    prev = ptr;
	    ptr = ptr->next;
	    if (ptr->dist < prev->dist) {
		done = 0;
		if (pprev) {
		    prev->next = ptr->next;
		    pprev->next = ptr;
		    ptr->next = prev;
		} else {
		    prev->next = ptr->next;
		    ptr->next = prev;
		    (*root) = ptr;
		}
	    }
	}
    }
}


static int
classify_sketch_loops(struct bu_ptbl *loopa, struct bu_ptbl *loopb, struct rt_sketch_internal *ip)
{
    struct loop_inter *inter_root=NULL, *ptr=NULL, *tmp=NULL;
    struct bn_tol tol;
    point2d_t pta = V2INIT_ZERO;
    point2d_t ptb = V2INIT_ZERO;
    point2d_t dir = V2INIT_ZERO;
    void *seg = NULL;
    fastf_t inv_len = 0.0;
    int loopa_count = 0, loopb_count = 0;
    int ret=UNKNOWN;

    BU_CK_PTBL(loopa);
    BU_CK_PTBL(loopb);
    RT_SKETCH_CK_MAGIC(ip);

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1.0e-6;
    tol.para = 1.0 - tol.perp;

    /* find points on a midpoint of a segment for each loop */
    seg = (void *)BU_PTBL_GET(loopa, 0);
    if (get_seg_midpoint(seg, ip, pta) < 0)
	return ret;
    seg = (void *)BU_PTBL_GET(loopb, 0);
    if (get_seg_midpoint(seg, ip, ptb) < 0)
	return ret;

    V2SUB2(dir, ptb, pta);
    inv_len = 1.0 / sqrt(MAG2SQ(dir));
    V2SCALE(dir, dir, inv_len);

    /* intersect pta<->ptb line with both loops */
    isect_2D_loop_ray(pta, dir, loopa, &inter_root, LOOPA, ip, &tol);
    isect_2D_loop_ray(pta, dir, loopb, &inter_root, LOOPB, ip, &tol);

    sort_intersections(&inter_root, &tol);

    /* examine intercepts to determine loop relationship */
    ptr = inter_root;
    while (ptr) {
	tmp = ptr;
	if (ret == UNKNOWN) {
	    if (ptr->which_loop == LOOPA) {
		loopa_count++;
		if (loopa_count && loopb_count) {
		    if (loopb_count % 2) {
			ret = A_IN_B;
		    } else {
			ret = DISJOINT;
		    }
		}
	    } else {
		loopb_count++;
		if (loopa_count && loopb_count) {
		    if (loopa_count % 2) {
			ret = B_IN_A;
		    } else {
			ret = DISJOINT;
		    }
		}
	    }
	}
	ptr = ptr->next;
	bu_free((char *)tmp, "loop intercept");
    }

    return ret;
}


/*
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_extrude_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    struct bu_list vhead;
    struct shell *s;
    struct faceuse *fu;
    struct vertex ***verts;
    struct vertex **vertsa;
    struct rt_extrude_internal *extrude_ip;
    struct rt_sketch_internal *sketch_ip;
    struct rt_curve *crv = NULL;
    struct bu_ptbl *aloop=NULL, loops, **containing_loops, *outer_loop;
    plane_t pl;
    int *used_seg;
    size_t i, j, k;
    size_t vert_count = 0;
    struct bv_vlist *vlp;

    RT_CK_DB_INTERNAL(ip);
    RT_VLFREE_INIT();
    struct bu_list *vlfree = &rt_vlfree;
    extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extrude_ip);

    if (!extrude_ip->skt) {
	bu_log("rt_extrude_tess: ERROR: no sketch for extrusion!\n");
	return -1;
    }

    sketch_ip = extrude_ip->skt;
    RT_SKETCH_CK_MAGIC(sketch_ip);

    crv = &sketch_ip->curve;

    if (crv->count < 1)
	return 0;

    /* find all the loops */
    used_seg = (int *)bu_calloc(crv->count, sizeof(int), "used_seg");
    bu_ptbl_init(&loops, 5, "loops");
    for (i = 0; i < crv->count; i++) {
	void *cur_seg;
	int loop_start = 0, loop_end = 0;
	int seg_start = 0, seg_end = 0;

	if (used_seg[i])
	    continue;

	BU_ALLOC(aloop, struct bu_ptbl);
	bu_ptbl_init(aloop, 5, "aloop");

	bu_ptbl_ins(aloop, (long *)crv->segment[i]);
	used_seg[i] = 1;
	cur_seg = crv->segment[i];
	get_indices(cur_seg, &loop_start, &loop_end);

	while (loop_end != loop_start) {
	    int added_seg;

	    added_seg = 0;
	    for (j = 0; j < crv->count; j++) {
		if (used_seg[j])
		    continue;

		get_indices(crv->segment[j], &seg_start, &seg_end);
		if (seg_start != seg_end && seg_start == loop_end) {
		    added_seg++;
		    bu_ptbl_ins(aloop, (long *)crv->segment[j]);
		    used_seg[j] = 1;
		    loop_end = seg_end;
		    if (loop_start == loop_end)
			break;
		}
	    }
	    if (!added_seg) {
		bu_log("rt_extrude_tess: A loop is not closed in sketch %s\n",
		       extrude_ip->sketch_name);
		bu_log("\ttessellation failed!!\n");
		for (j = 0; j < (size_t)BU_PTBL_LEN(&loops); j++) {
		    aloop = (struct bu_ptbl *)BU_PTBL_GET(&loops, j);
		    bu_ptbl_free(aloop);
		    bu_free((char *)aloop, "aloop");
		}
		bu_ptbl_free(&loops);
		bu_free((char *)used_seg, "used_seg");
		return -2;
	    }
	}
	bu_ptbl_ins(&loops, (long *)aloop);
    }
    bu_free((char *)used_seg, "used_seg");

    /* sort the loops to find inside/outside relationships */
    containing_loops = (struct bu_ptbl **)bu_calloc(BU_PTBL_LEN(&loops),
						    sizeof(struct bu_ptbl *), "containing_loops");
    for (i = 0; i < (size_t)BU_PTBL_LEN(&loops); i++) {
	BU_ALLOC(containing_loops[i], struct bu_ptbl);
	bu_ptbl_init(containing_loops[i], BU_PTBL_LEN(&loops), "containing_loops[i]");
    }

    for (i = 0; i < (size_t)BU_PTBL_LEN(&loops); i++) {
	struct bu_ptbl *loopa;

	loopa = (struct bu_ptbl *)BU_PTBL_GET(&loops, i);
	for (j=i+1; j<(size_t)BU_PTBL_LEN(&loops); j++) {
	    struct bu_ptbl *loopb;

	    loopb = (struct bu_ptbl *)BU_PTBL_GET(&loops, j);
	    switch (classify_sketch_loops(loopa, loopb, sketch_ip)) {
		case A_IN_B:
		    bu_ptbl_ins(containing_loops[i], (long *)loopb);
		    break;
		case B_IN_A:
		    bu_ptbl_ins(containing_loops[j], (long *)loopa);
		    break;
		case DISJOINT:
		    break;
		default:
		    bu_log("rt_extrude_tess: Failed to classify loops!!\n");
		    goto failed;
	    }
	}
    }

    /* make loops */

    /* find an outermost loop */
    outer_loop = (struct bu_ptbl *)NULL;
    for (i = 0; i < (size_t)BU_PTBL_LEN(&loops); i++) {
	if (BU_PTBL_LEN(containing_loops[i]) == 0) {
	    outer_loop = (struct bu_ptbl *)BU_PTBL_GET(&loops, i);
	    break;
	}
    }

    if (!outer_loop) {
	bu_log("No outer loop in sketch %s\n", extrude_ip->sketch_name);
	bu_log("\ttessellation failed\n");
	for (i = 0; i < (size_t)BU_PTBL_LEN(&loops); i++) {
	    aloop = (struct bu_ptbl *)BU_PTBL_GET(&loops, i);
	    bu_ptbl_free(aloop);
	    bu_free((char *)aloop, "aloop");
	    bu_ptbl_free(containing_loops[i]);
	    bu_free((char *)containing_loops[i], "aloop");
	}
	bu_ptbl_free(&loops);
	bu_free((char *)containing_loops, "containing_loops");
    }

    BU_LIST_INIT(&vhead);

    for (i = 0; outer_loop && i<(size_t)BU_PTBL_LEN(outer_loop); i++) {
	void *seg;

	seg = (void *)BU_PTBL_GET(outer_loop, i);
	if (seg_to_vlist(vlfree, &vhead, ttol, extrude_ip->V, extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, seg))
	    goto failed;
    }

    /* count vertices */
    vert_count = 0;
    for (BU_LIST_FOR (vlp, bv_vlist, &vhead)) {
	for (i = 0; i < vlp->nused; i++) {
	    if (vlp->cmd[i] == BV_VLIST_LINE_DRAW)
		vert_count++;
	}
    }

    *r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &((*r)->s_hd));

    /* make initial face from outer_loop */
    verts = (struct vertex ***)bu_calloc(vert_count, sizeof(struct vertex **), "verts");
    for (i = 0; i < vert_count; i++) {
	verts[i] = (struct vertex **)bu_calloc(1, sizeof(struct vertex *), "verts[i]");
    }

    fu = nmg_cmface(s, verts, vert_count);
    j = 0;
    for (BU_LIST_FOR (vlp, bv_vlist, &vhead)) {
	for (i = 0; i < vlp->nused; i++) {
	    if (vlp->cmd[i] == BV_VLIST_LINE_DRAW) {
		nmg_vertex_gv(*verts[j], vlp->pt[i]);
		j++;
	    }
	}
    }
    BV_FREE_VLIST(vlfree, &vhead);

    /* make sure face normal is in correct direction */
    bu_free((char *)verts, "verts");
    if (nmg_calc_face_plane(fu, pl, vlfree)) {
	bu_log("Failed to calculate face plane for extrusion\n");
	return -1;
    }
    nmg_face_g(fu, pl);
    if (VDOT(pl, extrude_ip->h) > 0.0) {
	nmg_reverse_face(fu);
	fu = fu->fumate_p;
    }

    /* add the rest of the loops */
    for (i = 0; i < (size_t)BU_PTBL_LEN(&loops); i++) {
	int fdir;
	vect_t cross;
	fastf_t pt_count = 0.0;
	fastf_t dot;
	int rev = 0;

	aloop = (struct bu_ptbl *)BU_PTBL_GET(&loops, i);
	if (aloop == outer_loop)
	    continue;

	if (BU_PTBL_LEN(containing_loops[i]) % 2) {
	    fdir = OT_OPPOSITE;
	} else {
	    fdir = OT_SAME;
	}

	for (j = 0; j < (size_t)BU_PTBL_LEN(aloop); j++) {
	    void *seg;

	    seg = (void *)BU_PTBL_GET(aloop, j);
	    if (seg_to_vlist(vlfree, &vhead, ttol, extrude_ip->V,
			     extrude_ip->u_vec, extrude_ip->v_vec, sketch_ip, seg))
		goto failed;
	}

	/* calculate plane of this loop */
	VSETALLN(pl, 0.0, 4);
	for (BU_LIST_FOR (vlp, bv_vlist, &vhead)) {
	    for (j = 0; j < vlp->nused; j++) {
		if (vlp->cmd[j] == BV_VLIST_LINE_DRAW) {
		    VCROSS(cross, vlp->pt[j-1], vlp->pt[j]);
		    VADD2(pl, pl, cross);
		}
	    }
	}

	VUNITIZE(pl);

	for (BU_LIST_FOR (vlp, bv_vlist, &vhead)) {
	    for (j = 0; j < vlp->nused; j++) {
		if (vlp->cmd[j] == BV_VLIST_LINE_DRAW) {
		    pl[W] += VDOT(pl, vlp->pt[j]);
		    pt_count++;
		}
	    }
	}
	pl[W] /= pt_count;

	dot = -VDOT(pl, extrude_ip->h);
	rev = 0;
	if (fdir == OT_SAME && dot < 0.0)
	    rev = 1;
	else if (fdir == OT_OPPOSITE && dot > 0.0)
	    rev = 1;

	vertsa = (struct vertex **)bu_calloc((int)pt_count, sizeof(struct vertex *), "verts");

	fu = nmg_add_loop_to_face(s, fu, vertsa, (int)pt_count, fdir);

	k = 0;
	for (BU_LIST_FOR (vlp, bv_vlist, &vhead)) {
	    for (j = 0; j < vlp->nused; j++) {
		if (vlp->cmd[j] == BV_VLIST_LINE_DRAW) {
		    if (rev) {
			nmg_vertex_gv(vertsa[(int)(pt_count) - k - 1], vlp->pt[j]);
		    } else {
			nmg_vertex_gv(vertsa[k], vlp->pt[j]);
		    }
		    k++;
		}
	    }
	}
	BV_FREE_VLIST(vlfree, &vhead);
    }

    /* extrude this face */
    if (nmg_extrude_face(fu, extrude_ip->h, vlfree, tol)) {
	bu_log("Failed to extrude face sketch\n");
	return -1;
    }

    nmg_region_a(*r, tol);

    return 0;

 failed:
    for (i = 0; i < (size_t)BU_PTBL_LEN(&loops); i++) {
	bu_ptbl_free(containing_loops[i]);
	bu_free((char *)containing_loops[i], "containing_loops[i]");
    }
    bu_free((char *)containing_loops, "containing_loops");
    bu_ptbl_free(aloop);
    bu_free((char *)aloop, "aloop");
    return -1;
}


/**
 * Import an EXTRUDE from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_extrude_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_extrude_internal *extrude_ip;
    struct rt_db_internal tmp_ip;
    struct directory *dp;
    char *sketch_name;
    union record *rp;
    char *ptr;

    /* must be double for import and export */
    double tmp_vec[ELEMENTS_PER_VECT];

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_EXTR) {
	bu_log("rt_extrude_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_EXTRUDE;
    ip->idb_meth = &OBJ[ID_EXTRUDE];
    BU_ALLOC(ip->idb_ptr, struct rt_extrude_internal);

    extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
    extrude_ip->magic = RT_EXTRUDE_INTERNAL_MAGIC;

    sketch_name = (char *)rp + sizeof(struct extr_rec);
    if (!dbip) {
	extrude_ip->skt = (struct rt_sketch_internal *)NULL;
    } else if ((dp=db_lookup(dbip, sketch_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_log("rt_extrude_import4: ERROR: Cannot find sketch (%.16s) for extrusion (%.16s)\n",
	       sketch_name, rp->extr.ex_name);
	extrude_ip->skt = (struct rt_sketch_internal *)NULL;
    } else {
	RT_UNIRESOURCE_INIT();
	if (rt_db_get_internal(&tmp_ip, dp, dbip, bn_mat_identity, &rt_uniresource) != ID_SKETCH) {
	    bu_log("rt_extrude_import4: ERROR: Cannot import sketch (%.16s) for extrusion (%.16s)\n",
		   sketch_name, rp->extr.ex_name);
	    bu_free(ip->idb_ptr, "extrusion");
	    return -1;
	} else {
	    extrude_ip->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
	}
    }

    if (mat == NULL) mat = bn_mat_identity;
    bu_cv_ntohd((unsigned char *)tmp_vec, rp->extr.ex_V, ELEMENTS_PER_VECT);
    MAT4X3PNT(extrude_ip->V, mat, tmp_vec);
    bu_cv_ntohd((unsigned char *)tmp_vec, rp->extr.ex_h, ELEMENTS_PER_VECT);
    MAT4X3VEC(extrude_ip->h, mat, tmp_vec);
    bu_cv_ntohd((unsigned char *)tmp_vec, rp->extr.ex_uvec, ELEMENTS_PER_VECT);
    MAT4X3VEC(extrude_ip->u_vec, mat, tmp_vec);
    bu_cv_ntohd((unsigned char *)tmp_vec, rp->extr.ex_vvec, ELEMENTS_PER_VECT);
    MAT4X3VEC(extrude_ip->v_vec, mat, tmp_vec);
    extrude_ip->keypoint = ntohl(*(uint32_t *)&rp->extr.ex_key[0]);

    ptr = (char *)rp;
    ptr += sizeof(struct extr_rec);
    extrude_ip->sketch_name = (char *)bu_calloc(NAMESIZE+1, sizeof(char), "Extrude sketch name");
    bu_strlcpy(extrude_ip->sketch_name, ptr, NAMESIZE+1);

    return 0;			/* OK */
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_extrude_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_extrude_internal *extrude_ip;

    /* must be double for import and export */
    double tmp_vec[ELEMENTS_PER_VECT];

    union record *rec;
    unsigned char *ptr;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_EXTRUDE) return -1;
    extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extrude_ip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = 2*sizeof(union record);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "extrusion external");
    rec = (union record *)ep->ext_buf;

    rec->extr.ex_id = DBID_EXTR;

    VSCALE(tmp_vec, extrude_ip->V, local2mm);
    bu_cv_htond(rec->extr.ex_V, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT);
    VSCALE(tmp_vec, extrude_ip->h, local2mm);
    bu_cv_htond(rec->extr.ex_h, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT);
    VSCALE(tmp_vec, extrude_ip->u_vec, local2mm);
    bu_cv_htond(rec->extr.ex_uvec, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT);
    VSCALE(tmp_vec, extrude_ip->v_vec, local2mm);
    bu_cv_htond(rec->extr.ex_vvec, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT);
    *(uint32_t *)rec->extr.ex_key = htonl(extrude_ip->keypoint);
    *(uint32_t *)rec->extr.ex_count = htonl(1);

    ptr = (unsigned char *)rec;
    ptr += sizeof(struct extr_rec);

    bu_strlcpy((char *)ptr, extrude_ip->sketch_name, ep->ext_nbytes-sizeof(struct extr_rec));

    return 0;
}


/**
 * The name is added by the caller, in the usual place.
 */
int
rt_extrude_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_extrude_internal *extrude_ip;
    unsigned char *ptr;
    size_t rem;

    /* must be double for import and export */
    double tmp_vec[4][ELEMENTS_PER_VECT];

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_EXTRUDE) return -1;

    extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extrude_ip);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = (long)(4 * ELEMENTS_PER_VECT * SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_LONG + strlen(extrude_ip->sketch_name) + 1);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "extrusion external");

    ptr = (unsigned char *)ep->ext_buf;
    rem = ep->ext_nbytes;

    VSCALE(tmp_vec[0], extrude_ip->V, local2mm);
    VSCALE(tmp_vec[1], extrude_ip->h, local2mm);
    VSCALE(tmp_vec[2], extrude_ip->u_vec, local2mm);
    VSCALE(tmp_vec[3], extrude_ip->v_vec, local2mm);
    bu_cv_htond(ptr, (unsigned char *)tmp_vec, ELEMENTS_PER_VECT*4);

    ptr += ELEMENTS_PER_VECT * 4 * SIZEOF_NETWORK_DOUBLE;
    rem -= ELEMENTS_PER_VECT * 4 * SIZEOF_NETWORK_DOUBLE;

    *(uint32_t *)ptr = htonl(extrude_ip->keypoint);

    ptr += SIZEOF_NETWORK_LONG;
    rem -= SIZEOF_NETWORK_LONG;

    bu_strlcpy((char *)ptr, extrude_ip->sketch_name, rem);

    return 0;
}

int
rt_extrude_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !ip || !mat)
	return BRLCAD_OK;

    struct rt_extrude_internal *tip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(tip);
    struct rt_extrude_internal *top = (struct rt_extrude_internal *)rop->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(top);

    vect_t V, h, u_vec, v_vec;
    VMOVE(V, tip->V);
    VMOVE(h, tip->h);
    VMOVE(u_vec, tip->u_vec);
    VMOVE(v_vec, tip->v_vec);

    MAT4X3PNT(top->V, mat, V);
    MAT4X3VEC(top->h, mat, h);
    MAT4X3VEC(top->u_vec, mat, u_vec);
    MAT4X3VEC(top->v_vec, mat, v_vec);

    return BRLCAD_OK;
}


/**
 * Import an EXTRUDE from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_extrude_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip)
{
    struct rt_extrude_internal *extrude_ip;
    struct rt_db_internal tmp_ip;
    struct directory *dp;
    char *sketch_name;
    unsigned char *ptr;

    /* must be double for import and export */
    double tmp_vec[4][ELEMENTS_PER_VECT];

    BU_CK_EXTERNAL(ep);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_EXTRUDE;
    ip->idb_meth = &OBJ[ID_EXTRUDE];
    BU_ALLOC(ip->idb_ptr, struct rt_extrude_internal);

    extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
    extrude_ip->magic = RT_EXTRUDE_INTERNAL_MAGIC;

    ptr = (unsigned char *)ep->ext_buf;
    sketch_name = (char *)ptr + ELEMENTS_PER_VECT*4*SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_LONG;
    if (!dbip) {
	extrude_ip->skt = (struct rt_sketch_internal *)NULL;
    } else if ((dp=db_lookup(dbip, sketch_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_log("rt_extrude_import4: ERROR: Cannot find sketch (%s) for extrusion\n",
	       sketch_name);
	extrude_ip->skt = (struct rt_sketch_internal *)NULL;
    } else {
	RT_UNIRESOURCE_INIT();
	if (rt_db_get_internal(&tmp_ip, dp, dbip, bn_mat_identity, &rt_uniresource) != ID_SKETCH) {
	    bu_log("rt_extrude_import4: ERROR: Cannot import sketch (%s) for extrusion\n",
		   sketch_name);
	    bu_free(ip->idb_ptr, "extrusion");
	    return -1;
	} else {
	    extrude_ip->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;
	}
    }

    bu_cv_ntohd((unsigned char *)tmp_vec, ptr, ELEMENTS_PER_VECT*4);
    VMOVE(extrude_ip->V, tmp_vec[0]);
    VMOVE(extrude_ip->h, tmp_vec[1]);
    VMOVE(extrude_ip->u_vec, tmp_vec[2]);
    VMOVE(extrude_ip->v_vec, tmp_vec[3]);
    ptr += ELEMENTS_PER_VECT * 4 * SIZEOF_NETWORK_DOUBLE;
    extrude_ip->keypoint = ntohl(*(uint32_t *)ptr);
    ptr += SIZEOF_NETWORK_LONG;
    extrude_ip->sketch_name = bu_strdup((const char *)ptr);

    /* Apply transform */
    if (mat == NULL) mat = bn_mat_identity;
    return rt_extrude_mat(ip, mat, ip);
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_extrude_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_extrude_internal *extrude_ip;
    char buf[256];
    point_t V;
    vect_t h, u, v;

    extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extrude_ip);

    bu_vls_strcat(str, "2D extrude (EXTRUDE)\n");
    VSCALE(V, extrude_ip->V, mm2local);
    VSCALE(h, extrude_ip->h, mm2local);
    VSCALE(u, extrude_ip->u_vec, mm2local);
    VSCALE(v, extrude_ip->v_vec, mm2local);
    sprintf(buf, "\tV = (%g %g %g)\n\tH = (%g %g %g)\n\tu_dir = (%g %g %g)\n\tv_dir = (%g %g %g)\n",
	    V3INTCLAMPARGS(V),
	    V3INTCLAMPARGS(h),
	    V3INTCLAMPARGS(u),
	    V3INTCLAMPARGS(v));
    bu_vls_strcat(str, buf);

    if (!verbose)
	return 0;

    snprintf(buf, 256, "\tsketch name: %s\n", extrude_ip->sketch_name);
    bu_vls_strcat(str, buf);

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_extrude_ifree(struct rt_db_internal *ip)
{
    struct rt_extrude_internal *extrude_ip;
    struct rt_db_internal tmp_ip;

    RT_CK_DB_INTERNAL(ip);

    extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extrude_ip);
    if (extrude_ip->skt) {
	RT_DB_INTERNAL_INIT(&tmp_ip);
	tmp_ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	tmp_ip.idb_type = ID_SKETCH;
	tmp_ip.idb_ptr = (void *)extrude_ip->skt;
	tmp_ip.idb_meth = &OBJ[ID_SKETCH];
	tmp_ip.idb_meth->ft_ifree(&tmp_ip);
    }
    extrude_ip->magic = 0;	/* sanity */

    bu_free(extrude_ip->sketch_name, "Extrude sketch_name");
    bu_free((char *)extrude_ip, "extrude ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
}


int
rt_extrude_xform(
    struct rt_db_internal *op,
    const mat_t mat,
    struct rt_db_internal *ip,
    int release,
    struct db_i *dbip)
{
    struct rt_extrude_internal *eip, *eop;
    point_t tmp_vec;

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(eip);

    if (op != ip) {
	RT_DB_INTERNAL_INIT(op);
	BU_ALLOC(eop, struct rt_extrude_internal);
	eop->magic = RT_EXTRUDE_INTERNAL_MAGIC;
	eop->sketch_name = bu_strdup(eip->sketch_name);
	op->idb_ptr = (void *)eop;
	op->idb_meth = &OBJ[ID_EXTRUDE];
	op->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	op->idb_type = ID_EXTRUDE;
	if (ip->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_init(&op->idb_avs, ip->idb_avs.count, "avs");
	    bu_avs_merge(&op->idb_avs, &ip->idb_avs);
	}
    } else {
	eop = (struct rt_extrude_internal *)ip->idb_ptr;
    }

    MAT4X3PNT(tmp_vec, mat, eip->V);
    VMOVE(eop->V, tmp_vec);
    MAT4X3VEC(tmp_vec, mat, eip->h);
    VMOVE(eop->h, tmp_vec);
    MAT4X3VEC(tmp_vec, mat, eip->u_vec);
    VMOVE(eop->u_vec, tmp_vec);
    MAT4X3VEC(tmp_vec, mat, eip->v_vec);
    VMOVE(eop->v_vec, tmp_vec);
    eop->keypoint = eip->keypoint;

    if (release && ip != op) {
	eop->skt = eip->skt;
	eip->skt = (struct rt_sketch_internal *)NULL;
	rt_db_free_internal(ip);
    } else if (eip->skt) {
	eop->skt = rt_copy_sketch(eip->skt);
    } else {
	eop->skt = (struct rt_sketch_internal *)NULL;
    }

    return 0;
}


int
rt_extrude_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr, "V {%%f %%f %%f} H {%%f %%f %%f} A {%%f %%f %%f} B {%%f %%f %%f} S %%s K %%d");

    return BRLCAD_OK;
}


int
rt_extrude_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_extrude_internal *extr=(struct rt_extrude_internal *) intern->idb_ptr;

    RT_EXTRUDE_CK_MAGIC(extr);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "extrude");
	bu_vls_printf(logstr, " V {%.25g %.25g %.25g}", V3ARGS(extr->V));
	bu_vls_printf(logstr, " H {%.25g %.25g %.25g}", V3ARGS(extr->h));
	bu_vls_printf(logstr, " A {%.25g %.25g %.25g}", V3ARGS(extr->u_vec));
	bu_vls_printf(logstr, " B {%.25g %.25g %.25g}", V3ARGS(extr->v_vec));
	bu_vls_printf(logstr, " S %s", extr->sketch_name);
    } else if (*attr == 'V')
	bu_vls_printf(logstr, "%.25g %.25g %.25g", V3ARGS(extr->V));
    else if (*attr == 'H')
	bu_vls_printf(logstr, "%.25g %.25g %.25g", V3ARGS(extr->h));
    else if (*attr == 'A')
	bu_vls_printf(logstr, "%.25g %.25g %.25g", V3ARGS(extr->u_vec));
    else if (*attr == 'B')
	bu_vls_printf(logstr, "%.25g %.25g %.25g", V3ARGS(extr->v_vec));
    else if (*attr == 'S' || BU_STR_EQUAL(attr, "sk_name"))
	bu_vls_printf(logstr, "%s", extr->sketch_name);
    else {
	bu_vls_strcat(logstr, "ERROR: unrecognized attribute, must be V, H, A, B, or S!");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
rt_extrude_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_extrude_internal *extr;
    fastf_t *newval;
    fastf_t len;

    RT_CK_DB_INTERNAL(intern);
    extr = (struct rt_extrude_internal *)intern->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extr);

    while (argc >= 2) {
	int array_len=3;

	if (*argv[0] == 'V') {
	    newval = extr->V;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &newval, &array_len) != array_len) {
		bu_vls_printf(logstr, "ERROR: incorrect number of coordinates for vertex\n");
		return BRLCAD_ERROR;
	    }
	} else if (*argv[0] == 'H') {
	    newval = extr->h;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &newval, &array_len) !=
		array_len) {
		bu_vls_printf(logstr, "ERROR: incorrect number of coordinates for vector\n");
		return BRLCAD_ERROR;
	    }
	} else if (*argv[0] == 'A') {
	    newval = extr->u_vec;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &newval, &array_len) !=
		array_len) {
		bu_vls_printf(logstr, "ERROR: incorrect number of coordinates for vector\n");
		return BRLCAD_ERROR;
	    }

	    /* insure that u_vec and v_vec are the same length */
	    len = MAGNITUDE(extr->u_vec);
	    VUNITIZE(extr->v_vec);
	    VSCALE(extr->v_vec, extr->v_vec, len);
	} else if (*argv[0] == 'B') {
	    newval = extr->v_vec;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &newval, &array_len) != array_len) {
		bu_vls_printf(logstr, "ERROR: incorrect number of coordinates for vector\n");
		return BRLCAD_ERROR;
	    }
	    /* insure that u_vec and v_vec are the same length */
	    len = MAGNITUDE(extr->v_vec);
	    VUNITIZE(extr->u_vec);
	    VSCALE(extr->u_vec, extr->u_vec, len);
	} else if (*argv[0] =='K') {
	    extr->keypoint = atoi(argv[1]);
	} else if (*argv[0] == 'S' || BU_STR_EQUAL(argv[0], "sk_name")) {
	    if (extr->sketch_name)
		bu_free((char *)extr->sketch_name, "rt_extrude_tcladjust: sketch_name");
	    extr->sketch_name = bu_strdup(argv[1]);
	}

	argc -= 2;
	argv += 2;
    }

    if (extr->sketch_name == NULL)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

void
rt_extrude_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct rt_extrude_internal* ip;

    intern->idb_type = ID_EXTRUDE;
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;

    BU_ASSERT(&OBJ[intern->idb_type] == ftp);
    intern->idb_meth = ftp;

    BU_ALLOC(ip, struct rt_extrude_internal);
    intern->idb_ptr = (void *)ip;

    ip->magic = RT_EXTRUDE_INTERNAL_MAGIC;
    ip->sketch_name = bu_strdup("");
}


int
rt_extrude_params(struct pc_pc_set *ps, const struct rt_db_internal *ip)
{
    if (!ps) return 0;
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}

const char *
rt_extrude_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    if (!pt || !ip)
	return NULL;

    point_t mpt = VINIT_ZERO;
    struct rt_extrude_internal *extrude = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(extrude);

    static const char *default_keystr = "V";
    const char *k = (keystr) ? keystr : default_keystr;

    if (BU_STR_EQUAL(k, default_keystr)) {
	VMOVE(mpt, extrude->V);
	goto extrude_kpt_end;
    }

    if (BU_STR_EQUAL(k, "V1")) {
	if (extrude->skt && extrude->skt->verts) {
	    VJOIN2(mpt, extrude->V, extrude->skt->verts[0][0], extrude->u_vec, extrude->skt->verts[0][1], extrude->v_vec);
	    goto extrude_kpt_end;
	}
    }

    // No keystr matches - failed
    return NULL;

extrude_kpt_end:

    MAT4X3PNT(*pt, mat, mpt);

    return k;
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
