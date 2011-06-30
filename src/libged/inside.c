/*                        I N S I D E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/inside.c
 *
 * The inside command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "db.h"

#include "./ged_private.h"


static char *p_arb4[] = {
    "Enter thickness for face 123: ",
    "Enter thickness for face 124: ",
    "Enter thickness for face 234: ",
    "Enter thickness for face 134: ",
};


static char *p_arb5[] = {
    "Enter thickness for face 1234: ",
    "Enter thickness for face 125: ",
    "Enter thickness for face 235: ",
    "Enter thickness for face 345: ",
    "Enter thickness for face 145: ",
};


static char *p_arb6[] = {
    "Enter thickness for face 1234: ",
    "Enter thickness for face 2356: ",
    "Enter thickness for face 1564: ",
    "Enter thickness for face 125: ",
    "Enter thickness for face 346: ",
};


static char *p_arb7[] = {
    "Enter thickness for face 1234: ",
    "Enter thickness for face 567: ",
    "Enter thickness for face 145: ",
    "Enter thickness for face 2376: ",
    "Enter thickness for face 1265: ",
    "Enter thickness for face 3475: ",
};


static char *p_arb8[] = {
    "Enter thickness for face 1234: ",
    "Enter thickness for face 5678: ",
    "Enter thickness for face 1485: ",
    "Enter thickness for face 2376: ",
    "Enter thickness for face 1265: ",
    "Enter thickness for face 3478: ",
};


static char *p_tgcin[] = {
    "Enter thickness for base (AxB): ",
    "Enter thickness for top (CxD): ",
    "Enter thickness for side: ",
};


static char *p_partin[] = {
    "Enter thickness for body: ",
};


static char *p_rpcin[] = {
    "Enter thickness for front plate (contains V): ",
    "Enter thickness for back plate: ",
    "Enter thickness for top plate: ",
    "Enter thickness for body: ",
};


static char *p_rhcin[] = {
    "Enter thickness for front plate (contains V): ",
    "Enter thickness for back plate: ",
    "Enter thickness for top plate: ",
    "Enter thickness for body: ",
};


static char *p_epain[] = {
    "Enter thickness for top plate: ",
    "Enter thickness for body: ",
};


static char *p_ehyin[] = {
    "Enter thickness for top plate: ",
    "Enter thickness for body: ",
};


static char *p_etoin[] = {
    "Enter thickness for body: ",
};


static char *p_nmgin[] = {
    "Enter thickness for shell: ",
};


/* finds inside arbs */
static int
arbin(struct ged *gedp,
      struct rt_db_internal *ip,
      fastf_t thick[6],
      int nface,
      int cgtype,		/* # of points, 4..8 */
      plane_t planes[6])
{
    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
    point_t center_pt;
    int num_pts=8;	/* number of points to solve using rt_arb_3face_intersect */
    int i;

    RT_ARB_CK_MAGIC(arb);

    /* find reference point (center_pt[3]) to find direction of normals */
    rt_arb_centroid(center_pt, arb, cgtype);

    /* move new face planes for the desired thicknesses
     * don't do this yet for an arb7 */
    if (cgtype != 7) {
	for (i=0; i<nface; i++) {
	    if ((planes[i][W] - VDOT(center_pt, &planes[i][0])) > 0.0)
		thick[i] *= -1.0;
	    planes[i][W] += thick[i];
	}
    }

    if (cgtype == 5)
	num_pts = 4;	/* use rt_arb_3face_intersect for first 4 points */
    else if (cgtype == 7)
	num_pts = 0;	/* don't use rt_arb_3face_intersect for any points */

    /* find the new vertices by intersecting the new face planes */
    for (i=0; i<num_pts; i++) {
	if (rt_arb_3face_intersect(arb->pt[i], (const plane_t *)planes, cgtype, i*3) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "cannot find inside arb\n");
	    return GED_ERROR;
	}
    }

    /* The following is code for the special cases of arb5 and arb7
     * These arbs have a vertex that is the intersection of four planes, and
     * the inside solid may have a single vertex or an edge replacing this vertex
     */
    if (cgtype == 5) {
	/* Here we are only concerned with the one vertex where 4 planes intersect
	 * in the original solid
	 */
	point_t pt[4];
	fastf_t dist0, dist1;

	/* calculate the four possible intersect points */
	if (bn_mkpoint_3planes(pt[0], planes[1], planes[2], planes[3])) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot find inside arb5\n");
	    bu_vls_printf(gedp->ged_result_str, "Cannot find intersection of three planes for point 0:\n");
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[1]));
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[2]));
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[3]));
	    return GED_ERROR;
	}
	if (bn_mkpoint_3planes(pt[1], planes[2], planes[3], planes[4])) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot find inside arb5\n");
	    bu_vls_printf(gedp->ged_result_str, "Cannot find intersection of three planes for point 1:\n");
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[2]));
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[3]));
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[4]));
	    return GED_ERROR;
	}
	if (bn_mkpoint_3planes(pt[2], planes[3], planes[4], planes[1])) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot find inside arb5\n");
	    bu_vls_printf(gedp->ged_result_str, "Cannot find intersection of three planes for point 2:\n");
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[3]));
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[4]));
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[1]));
	    return GED_ERROR;
	}
	if (bn_mkpoint_3planes(pt[3], planes[4], planes[1], planes[2])) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot find inside arb5\n");
	    bu_vls_printf(gedp->ged_result_str, "Cannot find intersection of three planes for point 3:\n");
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[4]));
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[1]));
	    bu_vls_printf(gedp->ged_result_str, "\t%f %f %f %f\n", V4ARGS(planes[2]));
	    return GED_ERROR;
	}

	if (bn_pt3_pt3_equal(pt[0], pt[1], &gedp->ged_wdbp->wdb_tol)) {
	    /* if any two of the calculates intersection points are equal,
	     * then all four must be equal
	     */
	    for (i=4; i<8; i++)
		VMOVE(arb->pt[i], pt[0]);

	    return GED_OK;
	}

	/* There will be an edge where the four planes come together
	 * Two edges of intersection have been calculated
	 * pt[0]<->pt[2]
	 * pt[1]<->pt[3]
	 * the one closest to the non-invloved plane (planes[0]) is the
	 * one we want
	 */

	dist0 = DIST_PT_PLANE(pt[0], planes[0]);
	if (dist0 < 0.0)
	    dist0 = (-dist0);

	dist1 = DIST_PT_PLANE(pt[1], planes[0]);
	if (dist1 < 0.0)
	    dist1 = (-dist1);

	if (dist0 < dist1) {
	    VMOVE(arb->pt[5], pt[0]);
	    VMOVE(arb->pt[6], pt[0]);
	    VMOVE(arb->pt[4], pt[2]);
	    VMOVE(arb->pt[7], pt[2]);
	} else {
	    VMOVE(arb->pt[4], pt[3]);
	    VMOVE(arb->pt[5], pt[3]);
	    VMOVE(arb->pt[6], pt[1]);
	    VMOVE(arb->pt[7], pt[1]);
	}
    } else if (cgtype == 7) {
	struct model *m;
	struct nmgregion *r;
	struct shell *s = NULL;
	struct faceuse *fu;
	struct rt_tess_tol ttol;
	struct bu_ptbl vert_tab;
	struct rt_bot_internal *bot;

	ttol.magic = RT_TESS_TOL_MAGIC;
	ttol.abs = gedp->ged_wdbp->wdb_ttol.abs;
	ttol.rel = gedp->ged_wdbp->wdb_ttol.rel;
	ttol.norm = gedp->ged_wdbp->wdb_ttol.norm;

	/* Make a model to hold the inside solid */
	m = nmg_mm();

	/* get an NMG version of this arb7 */
	if (!rt_functab[ip->idb_type].ft_tessellate || rt_functab[ip->idb_type].ft_tessellate(&r, m, ip, &ttol, &gedp->ged_wdbp->wdb_tol)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot tessellate arb7\n");
	    rt_db_free_internal(ip);
	    return GED_ERROR;
	}

	/* move face planes */
	for (i=0; i<nface; i++) {
	    int found=0;

	    /* look for the face plane with the same geometry as the arb7 planes */
	    s = BU_LIST_FIRST(shell, &r->s_hd);
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		struct face_g_plane *fg;
		plane_t pl;

		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME)
		    continue;

		NMG_GET_FU_PLANE(pl, fu);
		if (bn_coplanar(planes[i], pl, &gedp->ged_wdbp->wdb_tol) > 0) {
		    /* found the NMG face geometry that matches arb face i */
		    found = 1;
		    fg = fu->f_p->g.plane_p;
		    NMG_CK_FACE_G_PLANE(fg);

		    /* move the face by distance "thick[i]" */
		    if (fu->f_p->flip)
			fg->N[3] += thick[i];
		    else
			fg->N[3] -= thick[i];

		    break;
		}
	    }
	    if (!found) {
		bu_vls_printf(gedp->ged_result_str, "Could not move face plane for arb7, face #%d\n", i);
		nmg_km(m);
		return GED_ERROR;
	    }
	}

	/* solve for new vertex geometry
	 * This does all the vertices
	 */
	bu_ptbl_init(&vert_tab, 64, "vert_tab");
	nmg_vertex_tabulate(&vert_tab, &m->magic);
	for (i=0; i<BU_PTBL_END(&vert_tab); i++) {
	    struct vertex *v;

	    v = (struct vertex *)BU_PTBL_GET(&vert_tab, i);
	    NMG_CK_VERTEX(v);

	    if (nmg_in_vert(v, 0, &gedp->ged_wdbp->wdb_tol)) {
		bu_vls_printf(gedp->ged_result_str, "Could not find coordinates for inside arb7\n");
		nmg_km(m);
		bu_ptbl_free(&vert_tab);
		return GED_ERROR;
	    }
	}
	bu_ptbl_free(&vert_tab);

	/* rebound model */
	nmg_rebound(m, &gedp->ged_wdbp->wdb_tol);

	nmg_extrude_cleanup(s, 0, &gedp->ged_wdbp->wdb_tol);

	/* free old ip pointer */
	rt_db_free_internal(ip);

	/* convert the NMG to a BOT */
	bot = (struct rt_bot_internal *)nmg_bot(s, &gedp->ged_wdbp->wdb_tol);
	nmg_km(m);

	/* put new solid in "ip" */
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_BOT;
	ip->idb_meth = &rt_functab[ID_BOT];
	ip->idb_ptr = (genptr_t)bot;
    }

    return GED_OK;
}


/* Calculates inside TGC
 *
 * thick[0] is thickness for base (AxB)
 * thick[1] is thickness for top (CxD)
 * thick[2] is thickness for side
 */
static int
tgcin(struct ged *gedp, struct rt_db_internal *ip, fastf_t thick[6])
{
    struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)ip->idb_ptr;
    vect_t norm;		/* unit vector normal to base */
    fastf_t normal_height;	/* height in direction normal to base */
    vect_t v, h;		/* parameters for inside TGC */
    point_t top;		/* vertex at top of inside TGC */
    fastf_t mag_a, mag_b, mag_c, mag_d; /* lengths of original semi-radii */
    fastf_t new_mag_a, new_mag_b, new_mag_c, new_mag_d; /* new lengths */
    vect_t unit_a, unit_b, unit_c, unit_d; /* unit vectors along semi radii */
    fastf_t ratio;

    RT_TGC_CK_MAGIC(tgc);

    VSETALL(unit_a, 0);
    VSETALL(unit_b, 0);
    VSETALL(unit_c, 0);
    VSETALL(unit_d, 0);

    VCROSS(norm, tgc->a, tgc->b);
    VUNITIZE(norm);

    normal_height = VDOT(norm, tgc->h);
    if (normal_height < 0.0) {
	normal_height = (-normal_height);
	VREVERSE(norm, norm);
    }

    if ((thick[0] + thick[1]) >= normal_height) {
	bu_vls_printf(gedp->ged_result_str, "TGC shorter than base and top thicknesses\n");
	return GED_ERROR;
    }

    mag_a = MAGNITUDE(tgc->a);
    mag_b = MAGNITUDE(tgc->b);
    mag_c = MAGNITUDE(tgc->c);
    mag_d = MAGNITUDE(tgc->d);

    if ((mag_a < VDIVIDE_TOL && mag_c < VDIVIDE_TOL) ||
	(mag_b < VDIVIDE_TOL && mag_d < VDIVIDE_TOL)) {
	bu_vls_printf(gedp->ged_result_str, "TGC is too small too create inside solid");
	return GED_ERROR;
    }

    if (mag_a >= VDIVIDE_TOL) {
	VSCALE(unit_a, tgc->a, 1.0/mag_a);
    } else if (mag_c >= VDIVIDE_TOL) {
	VSCALE(unit_a, tgc->c, 1.0/mag_c);
    }

    if (mag_c >= VDIVIDE_TOL) {
	VSCALE(unit_c, tgc->c, 1.0/mag_c);
    } else if (mag_a >= VDIVIDE_TOL) {
	VSCALE(unit_c, tgc->a, 1.0/mag_a);
    }

    if (mag_b >= VDIVIDE_TOL) {
	VSCALE(unit_b, tgc->b, 1.0/mag_b);
    } else if (mag_d >= VDIVIDE_TOL) {
	VSCALE(unit_b, tgc->d, 1.0/mag_d);
    }

    if (mag_d >= VDIVIDE_TOL) {
	VSCALE(unit_d, tgc->d, 1.0/mag_d);
    } else if (mag_c >= VDIVIDE_TOL) {
	VSCALE(unit_d, tgc->b, 1.0/mag_b);
    }

    /* Calculate new vertex from base thickness */
    if (!ZERO(thick[0])) {
	/* calculate new vertex using similar triangles */
	ratio = thick[0]/normal_height;
	VJOIN1(v, tgc->v, ratio, tgc->h);

	/* adjust lengths of a and c to account for new vertex position */
	new_mag_a = mag_a + (mag_c - mag_a)*ratio;
	new_mag_b = mag_b + (mag_d - mag_b)*ratio;
    } else {
	/* just copy the existing values */
	VMOVE(v, tgc->v);
	new_mag_a = mag_a;
	new_mag_b = mag_b;
    }

    /* calculate new height vector */
    if (!ZERO(thick[1])) {
	/* calculate new height vector using simialr triangles */
	ratio = thick[1]/normal_height;
	VJOIN1(top, tgc->v, 1.0 - ratio, tgc->h);

	/* adjust lengths of c and d */
	new_mag_c = mag_c + (mag_a - mag_c)*ratio;
	new_mag_d = mag_d + (mag_b - mag_d)*ratio;
    } else {
	/* just copy existing values */
	VADD2(top, tgc->v, tgc->h);
	new_mag_c = mag_c;
	new_mag_d = mag_d;
    }

    /* calculate new height vector based on new vertex and top */
    VSUB2(h, top, v);

    if (!ZERO(thick[2])) {
	/* ther is a side thickness */
	vect_t ctoa;	/* unit vector from tip of C to tip of A */
	vect_t dtob;	/* unit vector from tip of D to tip of B */
	point_t pt_a, pt_b, pt_c, pt_d;	/* points at tips of semi radii */
	fastf_t delta_ac, delta_bd;	/* radius change for thickness */
	fastf_t dot;	/* dot product */
	fastf_t ratio1, ratio2;

	if ((thick[2] >= new_mag_a || thick[2] >= new_mag_b) &&
	    (thick[2] >= new_mag_c || thick[2] >= new_mag_d)) {
	    /* can't make a small enough TGC */
	    bu_vls_printf(gedp->ged_result_str, "Side thickness too large\n");
	    return GED_ERROR;
	}

	/* approach this as two 2D problems. One is in the plane containing
	 * the a, h, and c vectors. The other is in the plane containing
	 * the b, h, and d vectors.
	 * In the ahc plane:
	 * Calculate the amount that both a and c must be changed to produce
	 * a normal thickness of thick[2]. Use the vector from tip of c to tip
	 * of a and the unit_a vector to get sine of angle that the normal
	 * side thickness makes with vector a (and so also with vector c).
	 * The amount vectors a and c must change is thick[2]/(cosine of that angle).
	 * Similar for the bhd plane.
	 */

	/* Calculate unit vectors from tips of c/d to tips of a/b */
	VJOIN1(pt_a, v, new_mag_a, unit_a);
	VJOIN1(pt_b, v, new_mag_b, unit_b);
	VJOIN2(pt_c, v, 1.0, h, new_mag_c, unit_c);
	VJOIN2(pt_d, v, 1.0, h, new_mag_d, unit_d);
	VSUB2(ctoa, pt_a, pt_c);
	VSUB2(dtob, pt_b, pt_d);
	VUNITIZE(ctoa);
	VUNITIZE(dtob);

	/* Calculate amount vectors a and c must change */
	dot = VDOT(ctoa, unit_a);
	delta_ac = thick[2]/sqrt(1.0 - dot*dot);

	/* Calculate amount vectors d and d must change */
	dot = VDOT(dtob, unit_b);
	delta_bd = thick[2]/sqrt(1.0 - dot*dot);

	if ((delta_ac > new_mag_a || delta_bd > new_mag_b) &&
	    (delta_ac > new_mag_c || delta_bd > new_mag_d)) {
	    /* Can't make TGC small enough */
	    bu_vls_printf(gedp->ged_result_str, "Side thickness too large\n");
	    return GED_ERROR;
	}

	/* Check if changes will make vectors a or d lengths negative */
	if (delta_ac >= new_mag_c || delta_bd >= new_mag_d) {
	    /* top vertex (height) must move. Calculate similar triangle ratios */
	    if (delta_ac >= new_mag_c)
		ratio1 = (new_mag_a - delta_ac)/(new_mag_a - new_mag_c);
	    else
		ratio1 = 1.0;

	    if (delta_bd >= new_mag_d)
		ratio2 = (new_mag_b - delta_bd)/(new_mag_b - new_mag_d);
	    else
		ratio2 = 1.0;

	    /* choose the smallest similar triangle for setting new top vertex */
	    if (ratio1 < ratio2)
		ratio = ratio1;
	    else
		ratio = ratio2;

	    if (ZERO(ratio1 - ratio) && ratio1 < 1.0) /* c vector must go to zero */
		new_mag_c = SQRT_SMALL_FASTF;
	    else if (ratio1 > ratio && ratio < 1.0) {
		/* vector d will go to zero, but vector c will not */

		/* calculate original length of vector c at new top vertex */
		new_mag_c = new_mag_c + (new_mag_a - new_mag_c)*(1.0 - ratio);

		/* now just subtract delta */
		new_mag_c -= delta_ac;
	    } else /* just change c vector length by delta */
		new_mag_c -= delta_ac;

	    if (ZERO(ratio2 - ratio) && ratio2 < 1.0) /* vector d must go to zero */
		new_mag_d = SQRT_SMALL_FASTF;
	    else if (ratio2 > ratio && ratio < 1.0) {
		/* calculate vector length at new top vertex */
		new_mag_d = new_mag_d + (new_mag_b - new_mag_d)*(1.0 - ratio);

		/* now just subtract delta */
		new_mag_d -= delta_bd;
	    } else /* just adjust length */
		new_mag_d -= delta_bd;

	    VSCALE(h, h, ratio);
	    new_mag_a -= delta_ac;
	    new_mag_b -= delta_bd;
	} else if (delta_ac >= new_mag_a || delta_bd >= new_mag_b) {
	    /* base vertex (v) must move */

	    /* Calculate similar triangle ratios */
	    if (delta_ac >= new_mag_a)
		ratio1 = (new_mag_c - delta_ac)/(new_mag_c - new_mag_a);
	    else
		ratio1 = 1.0;

	    if (delta_bd >= new_mag_b)
		ratio2 = (new_mag_d - delta_bd)/(new_mag_d - new_mag_b);
	    else
		ratio2 = 1.0;

	    /* select smallest triangle to set new base vertex */
	    if (ratio1 < ratio2)
		ratio = ratio1;
	    else
		ratio = ratio2;

	    if (ZERO(ratio1 - ratio) && ratio1 < 1.0) /* vector a must go to zero */
		new_mag_a = SQRT_SMALL_FASTF;
	    else if (ratio1 > ratio && ratio < 1.0) {
		/* calculate length of vector a if it were at new base location */
		new_mag_a = new_mag_c + (new_mag_a - new_mag_c)*ratio;

		/* now just subtract delta */
		new_mag_a -= delta_ac;
	    } else /* just subtract delta */
		new_mag_a -= delta_ac;

	    if (ZERO(ratio2 - ratio) && ratio2 < 1.0) /* vector b must go to zero */
		new_mag_b = SQRT_SMALL_FASTF;
	    else if (ratio2 > ratio && ratio < 1.0) {
		/* Calculate length of b if it were at new base vector */
		new_mag_b = new_mag_d + (new_mag_b - new_mag_d)*ratio;

		/* now just subtract delta */
		new_mag_b -= delta_bd;
	    } else /* just subtract delta */
		new_mag_b -= delta_bd;

	    /* adjust height vector using smallest similar triangle ratio */
	    VJOIN1(v, v, 1.0-ratio, h);
	    VSUB2(h, top, v);
	    new_mag_c -= delta_ac;
	    new_mag_d -= delta_bd;
	} else {
	    /* just change the vector lengths */
	    new_mag_a -= delta_ac;
	    new_mag_b -= delta_bd;
	    new_mag_c -= delta_ac;
	    new_mag_d -= delta_bd;
	}
    }

/* copy new values into the TGC */
    VMOVE(tgc->v, v);
    VMOVE(tgc->h, h);
    VSCALE(tgc->a, unit_a, new_mag_a);
    VSCALE(tgc->b, unit_b, new_mag_b);
    VSCALE(tgc->c, unit_c, new_mag_c);
    VSCALE(tgc->d, unit_d, new_mag_d);

    return GED_OK;
}


/* finds inside of torus */
static int
torin(struct ged *gedp, struct rt_db_internal *ip, fastf_t thick[6])
{
    struct rt_tor_internal *tor = (struct rt_tor_internal *)ip->idb_ptr;

    RT_TOR_CK_MAGIC(tor);
    if (ZERO(thick[0]))
	return GED_OK;

    if (thick[0] < 0) {
	if ((tor->r_h - thick[0]) > (tor->r_a + .01)) {
	    bu_vls_printf(gedp->ged_result_str, "cannot do: r2 > r1\n");
	    return GED_ERROR;
	}
    }
    if (thick[0] >= tor->r_h) {
	bu_vls_printf(gedp->ged_result_str, "cannot do: r2 <= 0\n");
	return GED_ERROR;
    }

    tor->r_h = tor->r_h - thick[0];

    return GED_OK;
}


/* finds inside ellg */
static int
ellgin(struct ged *gedp, struct rt_db_internal *ip, fastf_t thick[6])
{
    struct rt_ell_internal *ell = (struct rt_ell_internal *)ip->idb_ptr;
    int i, j, k, order[3];
    fastf_t mag[3], nmag[3];
    fastf_t ratio;

    if (thick[0] <= 0.0)
	return 0;
    thick[2] = thick[1] = thick[0];	/* uniform thickness */

    RT_ELL_CK_MAGIC(ell);
    mag[0] = MAGNITUDE(ell->a);
    mag[1] = MAGNITUDE(ell->b);
    mag[2] = MAGNITUDE(ell->c);

    for (i=0; i<3; i++) {
	order[i] = i;
    }

    for (i=0; i<2; i++) {
	k = i + 1;
	for (j=k; j<3; j++) {
	    if (mag[i] < mag[j])
		order[i] = j;
	}
    }

    if ((ratio = mag[order[1]] / mag[order[0]]) < .8)
	thick[order[1]] = thick[order[1]]/(1.016447*pow(ratio, .071834));
    if ((ratio = mag[order[2]] / mag[order[1]]) < .8)
	thick[order[2]] = thick[order[2]]/(1.016447*pow(ratio, .071834));

    for (i=0; i<3; i++) {
	if ((nmag[i] = mag[i] - thick[i]) <= 0.0)
	    bu_vls_printf(gedp->ged_result_str, "Warning: new vector [%d] length <= 0 \n", i);
    }
    VSCALE(ell->a, ell->a, nmag[0]/mag[0]);
    VSCALE(ell->b, ell->b, nmag[1]/mag[1]);
    VSCALE(ell->c, ell->c, nmag[2]/mag[2]);

    return GED_OK;
}


/* find inside of particle solid */
static int
partin(struct ged *UNUSED(gedp), struct rt_db_internal *ip, fastf_t *thick)
{
    struct rt_part_internal *part = (struct rt_part_internal *)ip->idb_ptr;

    RT_PART_CK_MAGIC(part);

    if (*thick >= part->part_vrad || *thick >= part->part_hrad)
	return 1;    /* BAD */

    part->part_vrad -= *thick;
    part->part_hrad -= *thick;

    return GED_OK;
}


/* finds inside of rpc, not quite right - r needs to be smaller */
static int
rpcin(struct ged *UNUSED(gedp), struct rt_db_internal *ip, fastf_t thick[4])
{
    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;
    fastf_t b;
    vect_t Bu, Hu, Ru;

    RT_RPC_CK_MAGIC(rpc);

    /* get unit coordinate axes */
    VMOVE(Bu, rpc->rpc_B);
    VMOVE(Hu, rpc->rpc_H);
    VCROSS(Ru, Hu, Bu);
    VUNITIZE(Bu);
    VUNITIZE(Hu);
    VUNITIZE(Ru);

    b = MAGNITUDE(rpc->rpc_B);
    VJOIN2(rpc->rpc_V, rpc->rpc_V, thick[0], Hu, thick[2], Bu);
    VSCALE(rpc->rpc_H, Hu, MAGNITUDE(rpc->rpc_H) - thick[0] - thick[1]);
    VSCALE(rpc->rpc_B, Bu, b - thick[2] - thick[3]);
#if 0
    bp = b - thick[2] - thick[3];
    rp = rpc->rpc_r - thick[3];	/* !!! ESTIMATE !!! */
    yp = rp * sqrt((bp - thick[2])/bp);
    VSET(Norm,
	 0.,
	 2 * bp * yp/(rp * rp),
	 -1.);
    VUNITIZE(Norm)
	th = thick[3] / Norm[Y];
    rpc->rpc_r -= th;
#endif
    rpc->rpc_r -= thick[3];

    return GED_OK;
}


/* XXX finds inside of rhc, not quite right */
static int
rhcin(struct ged *UNUSED(gedp), struct rt_db_internal *ip, fastf_t thick[4])
{
    struct rt_rhc_internal *rhc = (struct rt_rhc_internal *)ip->idb_ptr;
    vect_t Bn, Hn, Bu, Hu, Ru;

    RT_RHC_CK_MAGIC(rhc);

    VMOVE(Bn, rhc->rhc_B);
    VMOVE(Hn, rhc->rhc_H);

    /* get unit coordinate axes */
    VMOVE(Bu, Bn);
    VMOVE(Hu, Hn);
    VCROSS(Ru, Hu, Bu);
    VUNITIZE(Bu);
    VUNITIZE(Hu);
    VUNITIZE(Ru);

    VJOIN2(rhc->rhc_V, rhc->rhc_V, thick[0], Hu, thick[2], Bu);
    VSCALE(rhc->rhc_H, Hu, MAGNITUDE(rhc->rhc_H) - thick[0] - thick[1]);
    VSCALE(rhc->rhc_B, Bu, MAGNITUDE(rhc->rhc_B) - thick[2] - thick[3]);
    rhc->rhc_r -= thick[3];

    return GED_OK;
}


/* finds inside of epa, not quite right */
static int
epain(struct ged *UNUSED(gedp), struct rt_db_internal *ip, fastf_t thick[2])
{
    struct rt_epa_internal *epa = (struct rt_epa_internal *)ip->idb_ptr;
    vect_t Hu;

    RT_EPA_CK_MAGIC(epa);

    VMOVE(Hu, epa->epa_H);
    VUNITIZE(Hu);

    VJOIN1(epa->epa_V, epa->epa_V, thick[0], Hu);
    VSCALE(epa->epa_H, Hu, MAGNITUDE(epa->epa_H) - thick[0] - thick[1]);
    epa->epa_r1 -= thick[1];
    epa->epa_r2 -= thick[1];

    return GED_OK;
}


/* finds inside of ehy, not quite right, */
static int
ehyin(struct ged *UNUSED(gedp), struct rt_db_internal *ip, fastf_t thick[2])
{
    struct rt_ehy_internal *ehy = (struct rt_ehy_internal *)ip->idb_ptr;
    vect_t Hu;

    RT_EHY_CK_MAGIC(ehy);

    VMOVE(Hu, ehy->ehy_H);
    VUNITIZE(Hu);

    VJOIN1(ehy->ehy_V, ehy->ehy_V, thick[0], Hu);
    VSCALE(ehy->ehy_H, Hu, MAGNITUDE(ehy->ehy_H) - thick[0] - thick[1]);
    ehy->ehy_r1 -= thick[1];
    ehy->ehy_r2 -= thick[1];

    return GED_OK;
}


/* finds inside of eto */
static int
etoin(struct ged *UNUSED(gedp), struct rt_db_internal *ip, fastf_t thick[1])
{
    fastf_t c;
    struct rt_eto_internal *eto = (struct rt_eto_internal *)ip->idb_ptr;

    RT_ETO_CK_MAGIC(eto);

    c = 1. - thick[0]/MAGNITUDE(eto->eto_C);
    VSCALE(eto->eto_C, eto->eto_C, c);
    eto->eto_rd -= thick[0];

    return GED_OK;
}


/* find inside for NMG */
static int
nmgin(struct ged *gedp, struct rt_db_internal *ip, fastf_t thick)
{
    struct model *m;
    struct nmgregion *r;

    if (ip->idb_type != ID_NMG)
	return GED_ERROR;

    m = (struct model *)ip->idb_ptr;
    NMG_CK_MODEL(m);

    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    while (BU_LIST_NOT_HEAD(r, &m->r_hd)) {
	struct nmgregion *next_r;
	struct shell *s;

	NMG_CK_REGION(r);

	next_r = BU_LIST_PNEXT(nmgregion, &r->l);

	s = BU_LIST_FIRST(shell, &r->s_hd);
	while (BU_LIST_NOT_HEAD(s, &r->s_hd)) {
	    struct shell *next_s;

	    next_s = BU_LIST_PNEXT(shell, &s->l);

	    nmg_shell_coplanar_face_merge(s, &gedp->ged_wdbp->wdb_tol, 1);
	    if (!nmg_kill_cracks(s))
		(void)nmg_extrude_shell(s, thick, 0, 0, &gedp->ged_wdbp->wdb_tol);

	    s = next_s;
	}

	if (BU_LIST_IS_EMPTY(&r->s_hd))
	    nmg_kr(r);

	r = next_r;
    }

    if (BU_LIST_IS_EMPTY(&m->r_hd)) {
	bu_vls_printf(gedp->ged_result_str, "No inside created\n");
	nmg_km(m);
	return GED_ERROR;
    } else
	return GED_OK;
}


int
ged_inside_internal(struct ged *gedp, struct rt_db_internal *ip, int argc, const char *argv[], int arg, char *o_name)
{
    int i;
    struct directory *dp;
    int cgtype = 8;		/* cgtype ARB 4..8 */
    int nface;
    fastf_t thick[6];
    plane_t planes[6];
    char *newname;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (ip->idb_type == ID_ARB8) {
	/* find the comgeom arb type, & reorganize */
	int uvec[8], svec[11];
	struct bu_vls error_msg;

	if (rt_arb_get_cgtype(&cgtype, ip->idb_ptr, &gedp->ged_wdbp->wdb_tol, uvec, svec) == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: BAD ARB\n", o_name);
	    return GED_ERROR;
	}

	/* must find new plane equations to account for
	 * any editing in the es_mat matrix or path to this solid.
	 */
	bu_vls_init(&error_msg);
	if (rt_arb_calc_planes(&error_msg, ip->idb_ptr, cgtype, planes, &gedp->ged_wdbp->wdb_tol) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s\nrt_arb_calc_planes(%s): failed\n", bu_vls_addr(&error_msg), o_name);
	    bu_vls_free(&error_msg);
	    return GED_ERROR;
	}
	bu_vls_free(&error_msg);
    }

    /* "ip" is loaded with the outside solid data */

    /* get the inside solid name */
    if (argc < arg+1) {
	bu_vls_printf(gedp->ged_result_str, "Enter name of the inside solid: ");
	return GED_MORE;
    }
    if (db_lookup(gedp->ged_wdbp->dbip, argv[arg], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s already exists.\n", argv[0], argv[arg]);
	return GED_ERROR;
    }
    if (db_version(gedp->ged_wdbp->dbip) < 5 && (int)strlen(argv[arg]) > NAMESIZE) {
	bu_vls_printf(gedp->ged_result_str, "Database version 4 names are limited to %d characters\n", NAMESIZE);
	return GED_ERROR;
    }
    newname = (char *)argv[arg];
    ++arg;

    /* get thicknesses and calculate parameters for newrec */
    switch (ip->idb_type) {

	case ID_ARB8: {
	    char **prompt = p_arb6;
	    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;

	    nface = 6;

	    switch (cgtype) {
		case 8:
		    prompt = p_arb8;
		    break;

		case 7:
		    prompt = p_arb7;
		    break;

		case 6:
		    prompt = p_arb6;
		    nface = 5;
		    VMOVE(arb->pt[5], arb->pt[6]);
		    break;

		case 5:
		    prompt = p_arb5;
		    nface = 5;
		    break;

		case 4:
		    prompt = p_arb4;
		    nface = 4;
		    VMOVE(arb->pt[3], arb->pt[4]);
		    break;
	    }

	    for (i=0; i<nface; i++) {
		if (argc < arg+1) {
		    bu_vls_printf(gedp->ged_result_str, "%s", prompt[i]);
		    return GED_MORE;
		}
		thick[i] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
		++arg;
	    }

	    if (arbin(gedp, ip, thick, nface, cgtype, planes))
		return GED_ERROR;
	    break;
	}

	case ID_TGC:
	    for (i=0; i<3; i++) {
		if (argc < arg+1) {
		    bu_vls_printf(gedp->ged_result_str, "%s", p_tgcin[i]);
		    return GED_MORE;
		}
		thick[i] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
		++arg;
	    }

	    if (tgcin(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_ELL:
	    if (argc < arg+1) {
		bu_vls_printf(gedp->ged_result_str, "Enter desired thickness: ");
		return GED_MORE;
	    }
	    thick[0] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
	    ++arg;

	    if (ellgin(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_TOR:
	    if (argc < arg+1) {
		bu_vls_printf(gedp->ged_result_str, "Enter desired thickness: ");
		return GED_MORE;
	    }
	    thick[0] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
	    ++arg;

	    if (torin(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_PARTICLE:
	    for (i = 0; i < 1; i++) {
		if (argc < arg+1) {
		    bu_vls_printf(gedp->ged_result_str, "%s", p_partin[i]);
		    return GED_MORE;
		}
		thick[i] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
		++arg;
	    }

	    if (partin(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_RPC:
	    for (i = 0; i < 4; i++) {
		if (argc < arg+1) {
		    bu_vls_printf(gedp->ged_result_str, "%s", p_rpcin[i]);
		    return GED_MORE;
		}
		thick[i] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
		++arg;
	    }

	    if (rpcin(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_RHC:
	    for (i = 0; i < 4; i++) {
		if (argc < arg+1) {
		    bu_vls_printf(gedp->ged_result_str, "%s", p_rhcin[i]);
		    return GED_MORE;
		}
		thick[i] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
		++arg;
	    }

	    if (rhcin(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_EPA:
	    for (i = 0; i < 2; i++) {
		if (argc < arg+1) {
		    bu_vls_printf(gedp->ged_result_str, "%s", p_epain[i]);
		    return GED_MORE;
		}
		thick[i] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
		++arg;
	    }

	    if (epain(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_EHY:
	    for (i = 0; i < 2; i++) {
		if (argc < arg+1) {
		    bu_vls_printf(gedp->ged_result_str, "%s", p_ehyin[i]);
		    return GED_MORE;
		}
		thick[i] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
		++arg;
	    }

	    if (ehyin(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_ETO:
	    for (i = 0; i < 1; i++) {
		if (argc < arg+1) {
		    bu_vls_printf(gedp->ged_result_str, "%s", p_etoin[i]);
		    return GED_MORE;
		}
		thick[i] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
		++arg;
	    }

	    if (etoin(gedp, ip, thick))
		return GED_ERROR;
	    break;

	case ID_NMG:
	    if (argc < arg+1) {
		bu_vls_printf(gedp->ged_result_str, "%s", *p_nmgin);
		return GED_MORE;
	    }
	    thick[0] = atof(argv[arg]) * gedp->ged_wdbp->dbip->dbi_local2base;
	    ++arg;
	    if (nmgin(gedp,  ip, thick[0]))
		return GED_ERROR;
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "Cannot find inside for '%s' solid\n", rt_functab[ip->idb_type].ft_name);
	    return GED_ERROR;
    }

    /* Add to in-core directory */
    dp = db_diradd(gedp->ged_wdbp->dbip, newname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&ip->idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database alloc error, aborting\n", argv[0]);
	return GED_ERROR;
    }
    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, ip, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: Database write error, aborting\n", argv[0]);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%s", argv[2]);
    return GED_OK;
}


int
ged_inside(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *outdp;
    struct rt_db_internal intern;
    int arg = 1;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    RT_DB_INTERNAL_INIT(&intern);

    if (argc < arg+1) {
	bu_vls_printf(gedp->ged_result_str, "Enter name of outside solid: ");
	return GED_MORE;
    }
    if ((outdp = db_lookup(gedp->ged_wdbp->dbip,  argv[arg], LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s not found", argv[0], argv[arg]);
	return GED_ERROR;
    }
    ++arg;

    if (rt_db_get_internal(&intern, outdp, gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }

    return ged_inside_internal(gedp, &intern, argc, argv, arg, outdp->d_namep);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
