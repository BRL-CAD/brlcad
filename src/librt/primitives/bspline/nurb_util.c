/*                     N U R B _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2010 United States Government as represented by
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
/** @addtogroup nurb */
/** @{ */
/** @file nurb_util.c
 *
 * Utilities for NURB curves and surfaces.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

/**
 * Create a place holder for a nurb surface.
 */
struct face_g_snurb *
rt_nurb_new_snurb(int u_order, int v_order, int n_u, int n_v, int n_rows, int n_cols, int pt_type, struct resource *res)
{
    register struct face_g_snurb * srf;
    int pnum;

    if (res) RT_CK_RESOURCE(res);

    GET_SNURB(srf);
    srf->order[0] = u_order;
    srf->order[1] = v_order;
    srf->dir = RT_NURB_SPLIT_ROW;

    srf->u.k_size = n_u;
    srf->v.k_size = n_v;
    srf->s_size[0] = n_rows;
    srf->s_size[1] = n_cols;
    srf->pt_type = pt_type;

    pnum = sizeof (fastf_t) * n_rows * n_cols * RT_NURB_EXTRACT_COORDS(pt_type);

    srf->u.knots = (fastf_t *) bu_malloc (
	n_u * sizeof (fastf_t), "rt_nurb_new_snurb: u kv knot values");
    srf->v.knots = (fastf_t *) bu_malloc (
	n_v * sizeof (fastf_t), "rt_nurb_new_snurb: v kv knot values");
    srf->ctl_points = (fastf_t *) bu_malloc(
	pnum, "rt_nurb_new_snurb: control mesh points");

    return srf;
}


/**
 * Create a place holder for a new nurb curve.
 */
struct edge_g_cnurb *
rt_nurb_new_cnurb(int order, int n_knots, int n_pts, int pt_type)
{
    register struct edge_g_cnurb * crv;

    GET_CNURB(crv);
    crv->order = order;

    crv->k.k_size = n_knots;
    crv->k.knots = (fastf_t *)
	bu_malloc(n_knots * sizeof(fastf_t),
		  "rt_nurb_new_cnurb: knot values");

    crv->c_size = n_pts;
    crv->pt_type = pt_type;

    crv->ctl_points = (fastf_t *)
	bu_malloc(sizeof(fastf_t) * RT_NURB_EXTRACT_COORDS(pt_type) *
		  n_pts,
		  "rt_nurb_new_cnurb: mesh point values");

    return crv;
}


/**
 * R T _ N U R B _ C L E A N _ S N U R B
 *
 * Clean up the storage use of an snurb, but don't release the
 * pointer.  Often used by routines that allocate an array of nurb
 * pointers, or use automatic variables to hold one.
 */
void
rt_nurb_clean_snurb(struct face_g_snurb *srf, struct resource *res)
{
    NMG_CK_SNURB(srf);

    if (res) RT_CK_RESOURCE(res);

    bu_free((char *)srf->u.knots, "rt_nurb_clean_snurb() u.knots");
    bu_free((char *)srf->v.knots, "rt_nurb_free_snurb() v.knots");
    bu_free((char *)srf->ctl_points, "rt_nurb_free_snurb() ctl_points");

    /* Invalidate the structure */
    srf->u.knots = (fastf_t *)NULL;
    srf->v.knots = (fastf_t *)NULL;
    srf->ctl_points = (fastf_t *)NULL;
    srf->order[0] = srf->order[1] = -1;
    srf->l.magic = 0;
}


/**
 * R T _ N U R B _ F R E E _ S N U R B
 */
void
rt_nurb_free_snurb(struct face_g_snurb *srf, struct resource *res)
{
    NMG_CK_SNURB(srf);

    if (res) RT_CK_RESOURCE(res);

    /* assume that links to other surface and curves are already
     * deleted.
     */

    bu_free((char *)srf->u.knots, "rt_nurb_free_snurb: u kv knots");
    bu_free((char *)srf->v.knots, "rt_nurb_free_snurb: v kv knots");
    bu_free((char *)srf->ctl_points, "rt_nurb_free_snurb: mesh points");

    srf->l.magic = 0;
    bu_free((char *)srf, "rt_nurb_free_snurb: snurb struct");
}


/**
 * R T _ N U R B _ C L E A N _ C N U R B
 *
 * Clean up the storage use of a cnurb, but don't release the pointer.
 * Often used by routines that allocate an array of nurb pointers, or
 * use automatic variables to hold one.
 */
void
rt_nurb_clean_cnurb(struct edge_g_cnurb *crv)
{
    NMG_CK_CNURB(crv);
    bu_free((char*)crv->k.knots, "rt_nurb_free_cnurb: knots");
    bu_free((char*)crv->ctl_points, "rt_nurb_free_cnurb: control points");
    /* Invalidate the structure */
    crv->k.knots = (fastf_t *)NULL;
    crv->ctl_points = (fastf_t *)NULL;
    crv->c_size = 0;
    crv->order = -1;
    crv->l.magic = 0;
}


/**
 * R T _ N U R B _ F R E E _ C N U R B
 *
 * Release a cnurb and all the storage that it references.
 */
void
rt_nurb_free_cnurb(struct edge_g_cnurb *crv)
{
    NMG_CK_CNURB(crv);
    bu_free((char*)crv->k.knots, "rt_nurb_free_cnurb: knots");
    bu_free((char*)crv->ctl_points, "rt_nurb_free_cnurb: control points");
    crv->l.magic = 0;		/* sanity */
    bu_free((char*)crv, "rt_nurb_free_cnurb: cnurb struct");
}


void
rt_nurb_c_print(const struct edge_g_cnurb *crv)
{
    register fastf_t * ptr;
    int i, j;

    NMG_CK_CNURB(crv);
    bu_log("curve = {\n");
    bu_log("\tOrder = %d\n", crv->order);
    bu_log("\tKnot Vector = {\n\t\t");

    for (i = 0; i < crv->k.k_size; i++)
	bu_log("%10.8f ", crv->k.knots[i]);

    bu_log("\n\t}\n");
    bu_log("\t");
    rt_nurb_print_pt_type(crv->pt_type);
    bu_log("\tmesh = {\n");
    for (ptr = &crv->ctl_points[0], i= 0;
	 i < crv->c_size; i++, ptr += RT_NURB_EXTRACT_COORDS(crv->pt_type))
    {
	bu_log("\t\t");
	for (j = 0; j < RT_NURB_EXTRACT_COORDS(crv->pt_type); j++)
	    bu_log("%4.5f\t", ptr[j]);
	bu_log("\n");

    }
    bu_log("\t}\n}\n");


}


void
rt_nurb_s_print(char *c, const struct face_g_snurb *srf)
{

    bu_log("%s\n", c);

    bu_log("order %d %d\n", srf->order[0], srf->order[1]);

    bu_log("u knot vector \n");

    rt_nurb_pr_kv(&srf->u);

    bu_log("v knot vector \n");

    rt_nurb_pr_kv(&srf->v);

    rt_nurb_pr_mesh(srf);

}


void
rt_nurb_pr_kv(const struct knot_vector *kv)
{
    register fastf_t * ptr = kv->knots;
    int i;

    bu_log("[%d]\t", kv->k_size);


    for (i = 0; i < kv->k_size; i++) {
	bu_log("%2.5f  ", *ptr++);
    }
    bu_log("\n");
}


void
rt_nurb_pr_mesh(const struct face_g_snurb *m)
{
    int i, j, k;
    fastf_t * m_ptr = m->ctl_points;
    int evp = RT_NURB_EXTRACT_COORDS(m->pt_type);

    NMG_CK_SNURB(m);

    bu_log("\t[%d] [%d]\n", m->s_size[0], m->s_size[1]);

    for (i = 0; i < m->s_size[0]; i++) {
	for (j =0; j < m->s_size[1]; j++) {
	    bu_log("\t");

	    for (k = 0; k < evp; k++)
		bu_log("%f    ", m_ptr[k]);

	    bu_log("\n");
	    m_ptr += RT_NURB_EXTRACT_COORDS(m->pt_type);
	}
	bu_log("\n");
    }
}


void
rt_nurb_print_pt_type(int c)
{
    int rat;

    rat = RT_NURB_IS_PT_RATIONAL(c);

    if (RT_NURB_EXTRACT_PT_TYPE(c) == RT_NURB_PT_XY)
	bu_log("Point Type = RT_NURB_PT_XY");
    else
	if (RT_NURB_EXTRACT_PT_TYPE(c) == RT_NURB_PT_XYZ)
	    bu_log("Point Type = RT_NURB_PT_XYX");
	else
	    if (RT_NURB_EXTRACT_PT_TYPE(c) == RT_NURB_PT_UV)
		bu_log("Point Type = RT_NURB_PT_UV");

    if (rat)
	bu_log("W\n");
    else
	bu_log("\n");
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
