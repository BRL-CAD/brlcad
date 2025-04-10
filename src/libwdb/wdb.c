/*                           W D B . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2025 United States Government as represented by
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

/** @file libwdb/wdb.c
 *
 * Library for writing MGED databases from arbitrary procedures.
 * Assumes that some of the structure of such databases are known by
 * the calling routines.
 *
 * It is expected that this library will grow as experience is gained.
 * Routines for writing every permissible solid do not yet exist.
 *
 * Note that routines which are passed point_t or vect_t or mat_t
 * parameters (which are call-by-address) must be VERY careful to
 * leave those parameters unmodified (e.g., by scaling), so that the
 * calling routine is not surprised.
 *
 * Return codes of 0 are OK, -1 signal an error.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


int
mk_half(struct rt_wdb *wdbp, const char *name, const vect_t norm, fastf_t d)
{
    struct rt_half_internal *half;

    BU_ALLOC(half, struct rt_half_internal);
    half->magic = RT_HALF_INTERNAL_MAGIC;
    VMOVE(half->eqn, norm);
    half->eqn[3] = d;

    return wdb_export(wdbp, name, (void *)half, ID_HALF, mk_conv2mm);
}


int
mk_grip(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t center,
    const vect_t normal,
    const fastf_t magnitude)
{
    struct rt_grip_internal *grip;

    BU_ALLOC(grip, struct rt_grip_internal);
    grip->magic = RT_GRIP_INTERNAL_MAGIC;
    VMOVE(grip->center, center);
    VMOVE(grip->normal, normal);
    grip->mag = magnitude;

    return wdb_export(wdbp, name, (void *)grip, ID_GRIP, mk_conv2mm);
}


int
mk_rpp(struct rt_wdb *wdbp, const char *name, const point_t min, const point_t max)
{
    point_t pt8[8];

    VSET(pt8[0], max[X], min[Y], min[Z]);
    VSET(pt8[1], max[X], max[Y], min[Z]);
    VSET(pt8[2], max[X], max[Y], max[Z]);
    VSET(pt8[3], max[X], min[Y], max[Z]);

    VSET(pt8[4], min[X], min[Y], min[Z]);
    VSET(pt8[5], min[X], max[Y], min[Z]);
    VSET(pt8[6], min[X], max[Y], max[Z]);
    VSET(pt8[7], min[X], min[Y], max[Z]);

    return mk_arb8(wdbp, name, &pt8[0][X]);
}


int
mk_wedge(struct rt_wdb *wdbp, const char *name, const point_t vert, const vect_t xdirv, const vect_t zdirv, fastf_t xlen, fastf_t ylen, fastf_t zlen, fastf_t x_top_len)
{
    point_t pts[8];	/* vertices for the wedge */
    vect_t xvec;	/* x_axis vector */
    vect_t txvec;	/* top x_axis vector */
    vect_t yvec;	/* y-axis vector */
    vect_t zvec;	/* z-axis vector */
    vect_t x_unitv;	/* x-axis unit vector*/
    vect_t z_unitv;	/* z-axis unit vector */
    vect_t y_unitv;

    VMOVE(x_unitv, xdirv);
    VUNITIZE(x_unitv);
    VMOVE(z_unitv, zdirv);
    VUNITIZE(z_unitv);

    /* Make y_unitv */
    VCROSS(y_unitv, x_unitv, z_unitv);

    /* Scale all vectors. */
    VSCALE(xvec, x_unitv, xlen);
    VSCALE(txvec, x_unitv, x_top_len);
    VSCALE(zvec, z_unitv, zlen);
    VSCALE(yvec, y_unitv, ylen);

    /* Make bottom face */

    VMOVE(pts[0], vert);		/* Move given vertex into pts[0] */
    VADD2(pts[1], pts[0], xvec);	/* second vertex. */
    VADD2(pts[2], pts[1], yvec);	/* third vertex */
    VADD2(pts[3], pts[0], yvec);	/* fourth vertex */

    /* Make top face by extruding bottom face vertices */

    VADD2(pts[4], pts[0], zvec);	/* fifth vertex */
    VADD2(pts[5], pts[4], txvec);	/* sixth vertex */
    VADD2(pts[6], pts[5], yvec);	/* seventh vertex */
    VADD2(pts[7], pts[4], yvec);	/* eighth vertex */

    return mk_arb8(wdbp, name, &pts[0][X]);
}


int
mk_arb4(struct rt_wdb *wdbp, const char *name, const fastf_t *pts)


    /* [4*3] */
{
    point_t pt8[8];

    VMOVE(pt8[0], &pts[0*3]);
    VMOVE(pt8[1], &pts[1*3]);
    VMOVE(pt8[2], &pts[2*3]);
    VMOVE(pt8[3], &pts[2*3]);	/* shared point for base */

    VMOVE(pt8[4], &pts[3*3]);	/* top point */
    VMOVE(pt8[5], &pts[3*3]);
    VMOVE(pt8[6], &pts[3*3]);
    VMOVE(pt8[7], &pts[3*3]);

    return mk_arb8(wdbp, name, &pt8[0][X]);
}


int
mk_arb5(struct rt_wdb *wdbp, const char *name, const fastf_t *pts)


    /* [5*3] */
{
    point_t pt8[8];

    VMOVE(pt8[0], &pts[0*3]);
    VMOVE(pt8[1], &pts[1*3]);
    VMOVE(pt8[2], &pts[2*3]);
    VMOVE(pt8[3], &pts[3*3]);
    VMOVE(pt8[4], &pts[4*3]);
    VMOVE(pt8[5], &pts[4*3]);
    VMOVE(pt8[6], &pts[4*3]);
    VMOVE(pt8[7], &pts[4*3]);

    return mk_arb8(wdbp, name, &pt8[0][X]);
}


int
mk_arb6(struct rt_wdb *wdbp, const char *name, const fastf_t *pts)

    /* [6*3] */
{
    point_t pt8[8];

    VMOVE(pt8[0], &pts[0*3]);
    VMOVE(pt8[1], &pts[1*3]);
    VMOVE(pt8[2], &pts[2*3]);
    VMOVE(pt8[3], &pts[3*3]);
    VMOVE(pt8[4], &pts[4*3]);
    VMOVE(pt8[5], &pts[4*3]);
    VMOVE(pt8[6], &pts[5*3]);
    VMOVE(pt8[7], &pts[5*3]);

    return mk_arb8(wdbp, name, &pt8[0][X]);
}


int
mk_arb7(struct rt_wdb *wdbp, const char *name, const fastf_t *pts)

    /* [7*3] */
{
    point_t pt8[8];

    VMOVE(pt8[0], &pts[0*3]);
    VMOVE(pt8[1], &pts[1*3]);
    VMOVE(pt8[2], &pts[2*3]);
    VMOVE(pt8[3], &pts[3*3]);
    VMOVE(pt8[4], &pts[4*3]);
    VMOVE(pt8[5], &pts[5*3]);
    VMOVE(pt8[6], &pts[6*3]);
    VMOVE(pt8[7], &pts[4*3]); /* Shared with point 5, per g_arb.c*/

    return mk_arb8(wdbp, name, &pt8[0][X]);
}


int
mk_arb8(struct rt_wdb *wdbp, const char *name, const fastf_t *pts)


    /* [24] */
{
    int i;
    struct rt_arb_internal *arb;

    BU_ALLOC(arb, struct rt_arb_internal);
    arb->magic = RT_ARB_INTERNAL_MAGIC;
    for (i=0; i < 8; i++) {
	VMOVE(arb->pt[i], &pts[i*3]);
    }

    return wdb_export(wdbp, name, (void *)arb, ID_ARB8, mk_conv2mm);
}


int
mk_sph(struct rt_wdb *wdbp, const char *name, const point_t center, fastf_t radius)
{
    struct rt_ell_internal *ell;

    BU_ALLOC(ell, struct rt_ell_internal);
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(ell->v, center);
    VSET(ell->a, radius, 0, 0);
    VSET(ell->b, 0, radius, 0);
    VSET(ell->c, 0, 0, radius);

    return wdb_export(wdbp, name, (void *)ell, ID_ELL, mk_conv2mm);
}


int
mk_ell(struct rt_wdb *wdbp, const char *name, const point_t center, const vect_t a, const vect_t b, const vect_t c)
{
    struct rt_ell_internal *ell;

    BU_ALLOC(ell, struct rt_ell_internal);
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(ell->v, center);
    VMOVE(ell->a, a);
    VMOVE(ell->b, b);
    VMOVE(ell->c, c);

    return wdb_export(wdbp, name, (void *)ell, ID_ELL, mk_conv2mm);
}


int
mk_hyp(struct rt_wdb *wdbp, const char *name, const point_t vertex, const vect_t height_vector, const vect_t vectA, fastf_t magB, fastf_t base_neck_ratio)
{
    struct rt_hyp_internal *hyp;

    BU_ALLOC(hyp, struct rt_hyp_internal);
    hyp->hyp_magic = RT_HYP_INTERNAL_MAGIC;


    if ((MAGNITUDE(vectA) <= SQRT_SMALL_FASTF) || (magB <= SQRT_SMALL_FASTF))
	return -2;

    hyp->hyp_bnr = base_neck_ratio;
    hyp->hyp_b = magB;
    VMOVE(hyp->hyp_Hi, height_vector);
    VMOVE(hyp->hyp_Vi, vertex);
    VMOVE(hyp->hyp_A, vectA);

    if (MAGNITUDE(hyp->hyp_Hi) < RT_LEN_TOL
	|| MAGNITUDE(hyp->hyp_A) < RT_LEN_TOL
	|| hyp->hyp_b < RT_LEN_TOL
	|| hyp->hyp_bnr < RT_LEN_TOL) {
	bu_log("ERROR, height, axes, and distance to asymptotes must be greater than zero!\n");
	return -1;
    }

    if (!NEAR_ZERO (VDOT(hyp->hyp_Hi, hyp->hyp_A), RT_DOT_TOL)) {
	bu_log("ERROR, major axis must be perpendicular to height vector!\n");
	return -1;
    }

    if (base_neck_ratio >= 1 || base_neck_ratio <= 0) {
	bu_log("ERROR, neck to base ratio must be between 0 and 1!\n");
	return -1;
    }

    if (hyp->hyp_b > MAGNITUDE(hyp->hyp_A)) {
	vect_t majorAxis;
	fastf_t minorLen;

	minorLen = MAGNITUDE(hyp->hyp_A);
	VCROSS(majorAxis, hyp->hyp_Hi, hyp->hyp_A);
	VSCALE(hyp->hyp_A, majorAxis, hyp->hyp_b);
	hyp->hyp_b = minorLen;
    }

    return wdb_export(wdbp, name, (void *)hyp, ID_HYP, mk_conv2mm);
}


int
mk_tor(struct rt_wdb *wdbp, const char *name, const point_t center, const vect_t inorm, double r1, double r2)
{
    struct rt_tor_internal *tor;

    BU_ALLOC(tor, struct rt_tor_internal);
    tor->magic = RT_TOR_INTERNAL_MAGIC;
    VMOVE(tor->v, center);
    VMOVE(tor->h, inorm);
    tor->r_a = r1;
    tor->r_h = r2;

    return wdb_export(wdbp, name, (void *)tor, ID_TOR, mk_conv2mm);
}


int
mk_rcc(struct rt_wdb *wdbp, const char *name, const point_t base, const vect_t height, fastf_t radius)
{
    vect_t cross1, cross2;
    vect_t a, b;

    if (MAGSQ(height) <= SQRT_SMALL_FASTF)
	return -2;

    /* Create two mutually perpendicular vectors, perpendicular to H */
    bn_vec_ortho(cross1, height);
    VCROSS(cross2, cross1, height);
    VUNITIZE(cross2);

    VSCALE(a, cross1, radius);
    VSCALE(b, cross2, radius);

    return mk_tgc(wdbp, name, base, height, a, b, a, b);
}


int
mk_tgc(struct rt_wdb *wdbp, const char *name, const point_t base, const vect_t height, const vect_t a, const vect_t b, const vect_t c, const vect_t d)
{
    struct rt_tgc_internal *tgc;

    BU_ALLOC(tgc, struct rt_tgc_internal);
    tgc->magic = RT_TGC_INTERNAL_MAGIC;
    VMOVE(tgc->v, base);
    VMOVE(tgc->h, height);
    VMOVE(tgc->a, a);
    VMOVE(tgc->b, b);
    VMOVE(tgc->c, c);
    VMOVE(tgc->d, d);

    return wdb_export(wdbp, name, (void *)tgc, ID_TGC, mk_conv2mm);
}


int
mk_cone(struct rt_wdb *wdbp, const char *name, const point_t base, const vect_t dirv, fastf_t height, fastf_t base_radius, fastf_t nose_radius)
{
    vect_t a, avec;	/* one base radius vector */
    vect_t b, bvec;	/* another base radius vector */
    vect_t cvec;	/* nose radius vector */
    vect_t dvec;	/* another nose radius vector */
    vect_t h_unitv;	/* local copy of dirv */
    vect_t hgtv;	/* height vector */
    fastf_t f;

    if ((f = MAGNITUDE(dirv)) <= SQRT_SMALL_FASTF)
	return -2;
    f = 1/f;
    VSCALE(h_unitv, dirv, f);
    VSCALE(hgtv, h_unitv, height);

    /* Now make a, b, c, and d vectors. */

    bn_vec_ortho(a, h_unitv);
    VUNITIZE(a);
    VCROSS(b, h_unitv, a);
    VSCALE(avec, a, base_radius);
    VSCALE(bvec, b, base_radius);
    VSCALE(cvec, a, nose_radius);
    VSCALE(dvec, b, nose_radius);

    return mk_tgc(wdbp, name, base, hgtv, avec, bvec, cvec, dvec);
}


int
mk_trc_h(struct rt_wdb *wdbp, const char *name, const vect_t base, const vect_t height, fastf_t radbase, fastf_t radtop)
{
    vect_t cross1, cross2;
    vect_t a, b, c, d;

    if (MAGSQ(height) <= SQRT_SMALL_FASTF)
	return -2;

    /* Create two mutually perpendicular vectors, perpendicular to H */
    bn_vec_ortho(cross1, height);
    VCROSS(cross2, cross1, height);
    VUNITIZE(cross2);

    VSCALE(a, cross1, radbase);
    VSCALE(b, cross2, radbase);

    VSCALE(c, cross1, radtop);
    VSCALE(d, cross2, radtop);

    return mk_tgc(wdbp, name, base, height, a, b, c, d);
}


int
mk_trc_top(struct rt_wdb *wdbp, const char *name, const point_t ibase, const point_t itop, fastf_t radbase, fastf_t radtop)
{
    vect_t height;

    VSUB2(height, itop, ibase);
    return mk_trc_h(wdbp, name, ibase, height, radbase, radtop);
}


int
mk_rpc(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    double half_w)
{
    struct rt_rpc_internal *rpc;

    BU_ALLOC(rpc, struct rt_rpc_internal);
    rpc->rpc_magic = RT_RPC_INTERNAL_MAGIC;

    VMOVE(rpc->rpc_V, vert);
    VMOVE(rpc->rpc_H, height);
    VMOVE(rpc->rpc_B, breadth);
    rpc->rpc_r = half_w;

    return wdb_export(wdbp, name, (void *)rpc, ID_RPC, mk_conv2mm);
}


int
mk_rhc(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t half_w,
    fastf_t asymp)
{
    struct rt_rhc_internal *rhc;

    BU_ALLOC(rhc, struct rt_rhc_internal);
    rhc->rhc_magic = RT_RHC_INTERNAL_MAGIC;

    VMOVE(rhc->rhc_V, vert);
    VMOVE(rhc->rhc_H, height);
    VMOVE(rhc->rhc_B, breadth);
    rhc->rhc_r = half_w;
    rhc->rhc_c = asymp;

    return wdb_export(wdbp, name, (void *)rhc, ID_RHC, mk_conv2mm);
}


int
mk_epa(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t r1,
    fastf_t r2)
{
    struct rt_epa_internal *epa;

    BU_ALLOC(epa, struct rt_epa_internal);
    epa->epa_magic = RT_EPA_INTERNAL_MAGIC;

    VMOVE(epa->epa_V, vert);
    VMOVE(epa->epa_H, height);
    VMOVE(epa->epa_Au, breadth);
    epa->epa_r1 = r1;
    epa->epa_r2 = r2;

    return wdb_export(wdbp, name, (void *)epa, ID_EPA, mk_conv2mm);
}


int
mk_ehy(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t r1,
    fastf_t r2,
    fastf_t c)
{
    struct rt_ehy_internal *ehy;

    BU_ALLOC(ehy, struct rt_ehy_internal);
    ehy->ehy_magic = RT_EHY_INTERNAL_MAGIC;

    VMOVE(ehy->ehy_V, vert);
    VMOVE(ehy->ehy_H, height);
    VMOVE(ehy->ehy_Au, breadth);
    ehy->ehy_r1 = r1;
    ehy->ehy_r2 = r2;
    ehy->ehy_c = c;

    return wdb_export(wdbp, name, (void *)ehy, ID_EHY, mk_conv2mm);
}


int mk_hrt(struct rt_wdb *wdbp, const char *name, const point_t center, const vect_t x, const vect_t y, const vect_t z, const fastf_t dist)
{
    struct rt_hrt_internal *hrt;

    BU_ALLOC(hrt, struct rt_hrt_internal);
    hrt->hrt_magic = RT_HRT_INTERNAL_MAGIC;

    VMOVE(hrt->v, center);
    VMOVE(hrt->xdir, x);
    VMOVE(hrt->ydir, y);
    VMOVE(hrt->zdir, z);
    hrt->d = dist;

    return wdb_export(wdbp, name, (void *)hrt, ID_HRT, mk_conv2mm);
}


int
mk_eto(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t norm,
    const vect_t smajor,
    fastf_t rrot,
    fastf_t sminor)
{
    struct rt_eto_internal *eto;

    BU_ALLOC(eto, struct rt_eto_internal);
    eto->eto_magic = RT_ETO_INTERNAL_MAGIC;

    VMOVE(eto->eto_V, vert);
    VMOVE(eto->eto_N, norm);
    VMOVE(eto->eto_C, smajor);
    eto->eto_r = rrot;
    eto->eto_rd = sminor;

    return wdb_export(wdbp, name, (void *)eto, ID_ETO, mk_conv2mm);
}


int
mk_metaball(
    struct rt_wdb *wdbp,
    const char *name,
    const size_t nctlpt, /* number of control points */
    const int method,
    const fastf_t threshold,
    const fastf_t *verts[5])
{
    struct rt_metaball_internal *mb;
    size_t i;

    BU_ALLOC(mb, struct rt_metaball_internal);
    mb->magic = RT_METABALL_INTERNAL_MAGIC;
    mb->threshold = threshold > 0 ? threshold : 1.0;
    mb->method = method >= 0 ? method : 2;	/* default to Blinn blob */
    BU_LIST_INIT(&mb->metaball_ctrl_head);

    for (i = 0; i < nctlpt; i++) {
	if (rt_metaball_add_point (mb, (const point_t *)verts[i], verts[i][3], verts[i][4]) != 0) {
	    bu_log("something is fishy here in mk_metaball");
	    bu_bomb("AIIEEEEEEE");
	}
    }

    return wdb_export(wdbp, name, (void *)mb, ID_METABALL, mk_conv2mm);
}


int
mk_binunif (
    struct rt_wdb *wdbp,
    const char *name,
    const void *data,
    wdb_binunif data_type,
    long count)
{
    struct rt_db_internal intern;
    struct rt_binunif_internal *binunif;
    unsigned int minor_type = 0;
    int from_file = 0;
    size_t bytes = 0;
    int nosign = 0;

    switch (data_type) {
	case WDB_BINUNIF_FILE_FLOAT:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_FLOAT:
	    bytes = sizeof(float);
	    minor_type = DB5_MINORTYPE_BINU_FLOAT;
	    break;
	case WDB_BINUNIF_FILE_DOUBLE:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_DOUBLE:
	    bytes = sizeof(double);
	    minor_type = DB5_MINORTYPE_BINU_DOUBLE;
	    break;

	case WDB_BINUNIF_FILE_INT8:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_INT8:
	    bytes = 1;
	    break;
	case WDB_BINUNIF_FILE_UINT8:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_UINT8:
	    nosign = 1;
	    bytes = 1;
	    break;
	case WDB_BINUNIF_FILE_INT16:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_INT16:
	    bytes = 2;
	    break;
	case WDB_BINUNIF_FILE_UINT16:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_UINT16:
	    nosign = 1;
	    bytes = 2;
	    break;
	case WDB_BINUNIF_FILE_INT32:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_INT32:
	    bytes = 4;
	    break;
	case WDB_BINUNIF_FILE_UINT32:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_UINT32:
	    nosign = 1;
	    bytes = 4;
	    break;
	case WDB_BINUNIF_FILE_INT64:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_INT64:
	    bytes = 8;
	    break;
	case WDB_BINUNIF_FILE_UINT64:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_UINT64:
	    nosign = 1;
	    bytes = 8;
	    break;

	case WDB_BINUNIF_FILE_CHAR:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_CHAR:
	    bytes = sizeof(char);
	    break;
	case WDB_BINUNIF_FILE_UCHAR:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_UCHAR:
	    nosign = 1;
	    bytes = sizeof(unsigned char);
	    break;
	case WDB_BINUNIF_FILE_SHORT:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_SHORT:
	    bytes = sizeof(short);
	    break;
	case WDB_BINUNIF_FILE_USHORT:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_USHORT:
	    nosign = 1;
	    bytes = sizeof(unsigned short);
	    break;
	case WDB_BINUNIF_FILE_INT:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_INT:
	    bytes = sizeof(int);
	    break;
	case WDB_BINUNIF_FILE_UINT:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_UINT:
	    nosign = 1;
	    bytes = sizeof(unsigned int);
	    break;
	case WDB_BINUNIF_FILE_LONG:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_LONG:
	    bytes = sizeof(long);
	    break;
	case WDB_BINUNIF_FILE_ULONG:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_ULONG:
	    nosign = 1;
	    bytes = sizeof(unsigned long);
	    break;
	case WDB_BINUNIF_FILE_LONGLONG:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_LONGLONG:
	    bytes = sizeof(int64_t);
	    break;
	case WDB_BINUNIF_FILE_ULONGLONG:
	    from_file = 1;
	    /* fall through */
	case WDB_BINUNIF_ULONGLONG:
	    nosign = 1;
	    bytes = sizeof(uint64_t);
	    break;
	default:
	    bu_log("Unknown binunif data source type: %d", data_type);
	    return 1;
    }

    /* the floating point types already have their minor type set */
    if (!minor_type) {
	switch (bytes) {
	    case 1:
		if (nosign) {
		    minor_type = DB5_MINORTYPE_BINU_8BITINT_U;
		} else {
		    minor_type = DB5_MINORTYPE_BINU_8BITINT;
		}
		break;
	    case 2:
		if (nosign) {
		    minor_type = DB5_MINORTYPE_BINU_16BITINT_U;
		} else {
		    minor_type = DB5_MINORTYPE_BINU_16BITINT;
		}
		break;
	    case 4:
		if (nosign) {
		    minor_type = DB5_MINORTYPE_BINU_32BITINT_U;
		} else {
		    minor_type = DB5_MINORTYPE_BINU_32BITINT;
		}
		break;
	    case 8:
		if (nosign) {
		    minor_type = DB5_MINORTYPE_BINU_64BITINT_U;
		} else {
		    minor_type = DB5_MINORTYPE_BINU_64BITINT;
		}
		break;
	}
    }

    /* sanity check that our sizes are correct */
    if (bytes != db5_type_sizeof_h_binu(minor_type)) {
	bu_log("mk_binunif: size inconsistency found, bytes=%zu expecting bytes=%zu\n",
	       bytes, db5_type_sizeof_h_binu(minor_type));
	bu_log("Warning: the uniform-array binary data object was NOT created");
	return -1;
    }

    /* use the librt load-from-file routine? */
    if (from_file) {
	return rt_mk_binunif (wdbp, name, (char *)data, minor_type, count);
    }

    /* count must be non-negative */
    if (count < 0) {
	count = 0;
    }

    /* loading from data already in memory */
    BU_ALLOC(binunif, struct rt_binunif_internal);
    binunif->magic = RT_BINUNIF_INTERNAL_MAGIC;
    binunif->type = minor_type;
    binunif->count = count;
    binunif->u.int8 = (char *)bu_malloc(count * bytes, "init binunif container");

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BINARY_UNIF;
    intern.idb_type = minor_type;
    intern.idb_ptr = (void*)binunif;
    intern.idb_meth = &OBJ[ID_BINUNIF];
    memcpy(binunif->u.int8, data, count * bytes);
    return wdb_put_internal(wdbp, name, &intern, mk_conv2mm);
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
