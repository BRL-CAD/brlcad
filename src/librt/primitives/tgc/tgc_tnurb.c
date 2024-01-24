/*                     T G C _ T N U R B . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2023 United States Government as represented by
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
/** @file primitives/tgc/tgc_tnurb.c
 *
 * Truncated General Cone NMG nurb.
 *
 */
/** @} */

#include "common.h"

#include <stddef.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/cv.h"
#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../../librt_private.h"

#define RAT  M_SQRT1_2

static const fastf_t nmg_tgc_unitcircle[36] = {
    1.0, 0.0, 0.0, 1.0,
    RAT, -RAT, 0.0, RAT,
    0.0, -1.0, 0.0, 1.0,
    -RAT, -RAT, 0.0, RAT,
    -1.0, 0.0, 0.0, 1.0,
    -RAT, RAT, 0.0, RAT,
    0.0, 1.0, 0.0, 1.0,
    RAT, RAT, 0.0, RAT,
    1.0, 0.0, 0.0, 1.0
};

static const fastf_t nmg_uv_unitcircle[27] = {
    1.0,   .5,  1.0,
    RAT,  RAT,  RAT,
    .5,   1.0,  1.0,
    0.0,  RAT,  RAT,
    0.0,   .5,  1.0,
    0.0,  0.0,  RAT,
    .5,   0.0,  1.0,
    RAT,  0.0,  RAT,
    1.0,   .5,  1.0
};

static void
nmg_tgc_disk(struct faceuse *fu, fastf_t *rmat, fastf_t height, int flip)
{
    struct face_g_snurb * fg;
    struct loopuse * lu;
    struct edgeuse * eu;
    struct edge_g_cnurb * eg;
    fastf_t *mptr;
    int i;
    vect_t vect;
    point_t point;

    nmg_face_g_snurb(fu,
		     2, 2,  			/* u, v order */
		     4, 4,			/* number of knots */
		     NULL, NULL, 		/* initial knot vectors */
		     2, 2, 			/* n_rows, n_cols */
		     RT_NURB_MAKE_PT_TYPE(3, 2, 0),
		     NULL);			/* Initial mesh */

    fg = fu->f_p->g.snurb_p;


    fg->u.knots[0] = 0.0;
    fg->u.knots[1] = 0.0;
    fg->u.knots[2] = 1.0;
    fg->u.knots[3] = 1.0;

    fg->v.knots[0] = 0.0;
    fg->v.knots[1] = 0.0;
    fg->v.knots[2] = 1.0;
    fg->v.knots[3] = 1.0;

    if (flip) {
	fg->ctl_points[0] = 1.;
	fg->ctl_points[1] = -1.;
	fg->ctl_points[2] = height;

	fg->ctl_points[3] = -1;
	fg->ctl_points[4] = -1.;
	fg->ctl_points[5] = height;

	fg->ctl_points[6] = 1.;
	fg->ctl_points[7] = 1.;
	fg->ctl_points[8] = height;

	fg->ctl_points[9] = -1.;
	fg->ctl_points[10] = 1.;
	fg->ctl_points[11] = height;
    } else {

	fg->ctl_points[0] = -1.;
	fg->ctl_points[1] = -1.;
	fg->ctl_points[2] = height;

	fg->ctl_points[3] = 1;
	fg->ctl_points[4] = -1.;
	fg->ctl_points[5] = height;

	fg->ctl_points[6] = -1.;
	fg->ctl_points[7] = 1.;
	fg->ctl_points[8] = height;

	fg->ctl_points[9] = 1.;
	fg->ctl_points[10] = 1.;
	fg->ctl_points[11] = height;
    }

    /* multiple the matrix to get orientation and scaling that we want */
    mptr = fg->ctl_points;

    i = fg->s_size[0] * fg->s_size[1];

    for (; i> 0; i--) {
	MAT4X3PNT(vect, rmat, mptr);
	mptr[0] = vect[0];
	mptr[1] = vect[1];
	mptr[2] = vect[2];
	mptr += 3;
    }

    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
    NMG_CK_LOOPUSE(lu);
    eu= BU_LIST_FIRST(edgeuse, &lu->down_hd);
    NMG_CK_EDGEUSE(eu);


    if (!flip) {
	nmg_nurb_s_eval(fu->f_p->g.snurb_p,
			nmg_uv_unitcircle[0], nmg_uv_unitcircle[1], point);
	nmg_vertex_gv(eu->vu_p->v_p, point);
    } else {
	nmg_nurb_s_eval(fu->f_p->g.snurb_p,
			nmg_uv_unitcircle[12], nmg_uv_unitcircle[13], point);
	nmg_vertex_gv(eu->vu_p->v_p, point);
    }

    nmg_edge_g_cnurb(eu, 3, 12, NULL, 9, RT_NURB_MAKE_PT_TYPE(3, 3, 1),
		     NULL);

    eg = eu->g.cnurb_p;
    eg->order = 3;

    eg->k.knots[0] = 0.0;
    eg->k.knots[1] = 0.0;
    eg->k.knots[2] = 0.0;
    eg->k.knots[3] = .25;
    eg->k.knots[4] = .25;
    eg->k.knots[5] = .5;
    eg->k.knots[6] = .5;
    eg->k.knots[7] = .75;
    eg->k.knots[8] = .75;
    eg->k.knots[9] = 1.0;
    eg->k.knots[10] = 1.0;
    eg->k.knots[11] = 1.0;

    if (!flip) {
	for (i = 0; i < 27; i++)
	    eg->ctl_points[i] = nmg_uv_unitcircle[i];
    } else {

	VSET(&eg->ctl_points[0], 0.0, .5, 1.0);
	VSET(&eg->ctl_points[3], 0.0, 0.0, RAT);
	VSET(&eg->ctl_points[6], 0.5, 0.0, 1.0);
	VSET(&eg->ctl_points[9], RAT, 0.0, RAT);
	VSET(&eg->ctl_points[12], 1.0, .5, 1.0);
	VSET(&eg->ctl_points[15], RAT, RAT, RAT);
	VSET(&eg->ctl_points[18], .5, 1.0, 1.0);
	VSET(&eg->ctl_points[21], 0.0, RAT, RAT);
	VSET(&eg->ctl_points[24], 0.0, .5, 1.0);
    }
}




/* Create a cylinder with a top surface and a bottom surface
 * defined by the ellipsoids at the top and bottom of the
 * cylinder, the top_mat, and bot_mat are applied to a unit circle
 * for the top row of the surface and the bot row of the surface
 * respectively.
 */
static void
nmg_tgc_nurb_cyl(struct faceuse *fu, fastf_t *top_mat, fastf_t *bot_mat)
{
    struct face_g_snurb * fg;
    struct loopuse * lu;
    struct edgeuse * eu;
    fastf_t * mptr;
    int i;
    hvect_t vect;
    point_t uvw;
    point_t point;
    hvect_t hvect;

    nmg_face_g_snurb(fu,
		     3, 2,
		     12, 4,
		     NULL, NULL,
		     2, 9,
		     RT_NURB_MAKE_PT_TYPE(4, 3, 1),
		     NULL);

    fg = fu->f_p->g.snurb_p;

    fg->v.knots[0] = 0.0;
    fg->v.knots[1] = 0.0;
    fg->v.knots[2] = 1.0;
    fg->v.knots[3] = 1.0;

    fg->u.knots[0] = 0.0;
    fg->u.knots[1] = 0.0;
    fg->u.knots[2] = 0.0;
    fg->u.knots[3] = .25;
    fg->u.knots[4] = .25;
    fg->u.knots[5] = .5;
    fg->u.knots[6] = .5;
    fg->u.knots[7] = .75;
    fg->u.knots[8] = .75;
    fg->u.knots[9] = 1.0;
    fg->u.knots[10] = 1.0;
    fg->u.knots[11] = 1.0;

    mptr = fg->ctl_points;

    for (i = 0; i < 9; i++) {
	MAT4X4PNT(vect, top_mat, &nmg_tgc_unitcircle[i*4]);
	mptr[0] = vect[0];
	mptr[1] = vect[1];
	mptr[2] = vect[2];
	mptr[3] = vect[3];
	mptr += 4;
    }

    for (i = 0; i < 9; i++) {
	MAT4X4PNT(vect, bot_mat, &nmg_tgc_unitcircle[i*4]);
	mptr[0] = vect[0];
	mptr[1] = vect[1];
	mptr[2] = vect[2];
	mptr[3] = vect[3];
	mptr += 4;
    }

    /* Assign edgeuse parameters & vertex geometry */

    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
    NMG_CK_LOOPUSE(lu);
    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
    NMG_CK_EDGEUSE(eu);

    /* March around the fu's loop assigning uv parameter values */

    nmg_nurb_s_eval(fg, 0.0, 0.0, hvect);
    HDIVIDE(point, hvect);
    nmg_vertex_gv(eu->vu_p->v_p, point);	/* 0, 0 vertex */

    VSET(uvw, 0, 0, 0);
    nmg_vertexuse_a_cnurb(eu->vu_p, uvw);
    VSET(uvw, 1, 0, 0);
    nmg_vertexuse_a_cnurb(eu->eumate_p->vu_p, uvw);
    eu = BU_LIST_NEXT(edgeuse, &eu->l);

    VSET(uvw, 1, 0, 0);
    nmg_vertexuse_a_cnurb(eu->vu_p, uvw);
    VSET(uvw, 1, 1, 0);
    nmg_vertexuse_a_cnurb(eu->eumate_p->vu_p, uvw);
    eu = BU_LIST_NEXT(edgeuse, &eu->l);

    VSET(uvw, 1, 1, 0);
    nmg_vertexuse_a_cnurb(eu->vu_p, uvw);
    VSET(uvw, 0, 1, 0);
    nmg_vertexuse_a_cnurb(eu->eumate_p->vu_p, uvw);

    nmg_nurb_s_eval(fg, 1., 1., hvect);
    HDIVIDE(point, hvect);
    nmg_vertex_gv(eu->vu_p->v_p, point);		/* 4, 1 vertex */

    eu = BU_LIST_NEXT(edgeuse, &eu->l);

    VSET(uvw, 0, 1, 0);
    nmg_vertexuse_a_cnurb(eu->vu_p, uvw);
    VSET(uvw, 0, 0, 0);
    nmg_vertexuse_a_cnurb(eu->eumate_p->vu_p, uvw);

    /* Create the edge loop geometry */

    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
    NMG_CK_LOOPUSE(lu);
    eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
    NMG_CK_EDGEUSE(eu);

    for (i = 0; i < 4; i++) {
	nmg_edge_g_cnurb_plinear(eu);
	eu = BU_LIST_NEXT(edgeuse, &eu->l);
    }
}




/**
 * "Tessellate an TGC into a trimmed-NURB-NMG data structure.
 * Computing NURB surfaces and trimming curves to interpolate
 * the parameters of the TGC
 *
 * The process is to create the nmg topology of the TGC fill it
 * in with a unit cylinder geometry (i.e. unitcircle at the top (0, 0, 1)
 * unit cylinder of radius 1, and unitcircle at the bottom), and then
 * scale it with a perspective matrix derived from the parameters of the
 * tgc. The result is three trimmed nurb surfaces which interpolate the
 * parameters of the original TGC.
 *
 * Returns -
 * -1 failure
 * 0 OK. *r points to nmgregion that holds this tessellation
 */

int
rt_tgc_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_tgc_internal *tip;

    struct shell *s;
    struct vertex *verts[2];
    struct vertex **vertp[4];
    struct faceuse * top_fu;
    struct faceuse * cyl_fu;
    struct faceuse * bot_fu;
    vect_t uvw;
    struct loopuse *lu;
    struct edgeuse *eu;
    struct edgeuse *top_eu;
    struct edgeuse *bot_eu;

    mat_t mat;
    mat_t imat, omat, top_mat, bot_mat;
    vect_t anorm;
    vect_t bnorm;
    vect_t cnorm;


    /* Get the internal representation of the tgc */

    RT_CK_DB_INTERNAL(ip);
    tip = (struct rt_tgc_internal *) ip->idb_ptr;
    RT_TGC_CK_MAGIC(tip);

    /* Create the NMG Topology */

    *r = nmg_mrsv(m);	/* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);


    /* Create transformation matrix for the top cap surface*/

    MAT_IDN(omat);
    MAT_IDN(mat);

    omat[0] = MAGNITUDE(tip->c);
    omat[5] = MAGNITUDE(tip->d);
    omat[3] = tip->v[0] + tip->h[0];
    omat[7] = tip->v[1] + tip->h[1];
    omat[11] = tip->v[2] + tip->h[2];

    bn_mat_mul(imat, mat, omat);

    VMOVE(anorm, tip->c);
    VMOVE(bnorm, tip->d);
    VCROSS(cnorm, tip->c, tip->d);
    VUNITIZE(anorm);
    VUNITIZE(bnorm);
    VUNITIZE(cnorm);

    MAT_IDN(omat);

    VMOVE(&omat[0], anorm);
    VMOVE(&omat[4], bnorm);
    VMOVE(&omat[8], cnorm);


    bn_mat_mul(top_mat, omat, imat);

    /* Create topology for top cap surface */

    verts[0] = verts[1] = NULL;
    vertp[0] = &verts[0];
    top_fu = nmg_cmface(s, vertp, 1);

    lu = BU_LIST_FIRST(loopuse, &top_fu->lu_hd);
    NMG_CK_LOOPUSE(lu);
    eu= BU_LIST_FIRST(edgeuse, &lu->down_hd);
    NMG_CK_EDGEUSE(eu);
    top_eu = eu;

    VSET(uvw, 0, 0, 0);
    nmg_vertexuse_a_cnurb(eu->vu_p, uvw);
    VSET(uvw, 1, 0, 0);
    nmg_vertexuse_a_cnurb(eu->eumate_p->vu_p, uvw);

    /* Top cap surface */
    nmg_tgc_disk(top_fu, top_mat, 0.0, 0);

    /* Create transformation matrix for the bottom cap surface*/

    MAT_IDN(omat);
    MAT_IDN(mat);

    omat[0] = MAGNITUDE(tip->a);
    omat[5] = MAGNITUDE(tip->b);
    omat[3] = tip->v[0];
    omat[7] = tip->v[1];
    omat[11] = tip->v[2];

    bn_mat_mul(imat, mat, omat);

    VMOVE(anorm, tip->a);
    VMOVE(bnorm, tip->b);
    VCROSS(cnorm, tip->a, tip->b);
    VUNITIZE(anorm);
    VUNITIZE(bnorm);
    VUNITIZE(cnorm);

    MAT_IDN(omat);

    VMOVE(&omat[0], anorm);
    VMOVE(&omat[4], bnorm);
    VMOVE(&omat[8], cnorm);

    bn_mat_mul(bot_mat, omat, imat);

    /* Create topology for bottom cap surface */

    vertp[0] = &verts[1];
    bot_fu = nmg_cmface(s, vertp, 1);

    lu = BU_LIST_FIRST(loopuse, &bot_fu->lu_hd);
    NMG_CK_LOOPUSE(lu);
    eu= BU_LIST_FIRST(edgeuse, &lu->down_hd);
    NMG_CK_EDGEUSE(eu);
    bot_eu = eu;

    VSET(uvw, 0, 0, 0);
    nmg_vertexuse_a_cnurb(eu->vu_p, uvw);
    VSET(uvw, 1, 0, 0);
    nmg_vertexuse_a_cnurb(eu->eumate_p->vu_p, uvw);


    nmg_tgc_disk(bot_fu, bot_mat, 0.0, 1);

    /* Create topology for cylinder surface */

    vertp[0] = &verts[0];
    vertp[1] = &verts[0];
    vertp[2] = &verts[1];
    vertp[3] = &verts[1];
    cyl_fu = nmg_cmface(s, vertp, 4);

    nmg_tgc_nurb_cyl(cyl_fu, top_mat, bot_mat);

    /* fuse top cylinder edge to matching edge on body of cylinder */
    lu = BU_LIST_FIRST(loopuse, &cyl_fu->lu_hd);

    eu= BU_LIST_FIRST(edgeuse, &lu->down_hd);

    nmg_je(top_eu, eu);

    eu= BU_LIST_LAST(edgeuse, &lu->down_hd);
    eu= BU_LIST_LAST(edgeuse, &eu->l);

    nmg_je(bot_eu, eu);
    nmg_region_a(*r, tol);

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
