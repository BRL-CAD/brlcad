/*                          P N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

/** @file libwdb/pnts.c
 *
 * Support for writing a Point Set (pnts) primitive from arbitrary
 * procedures.
 *
 */

#include "common.h"

#include <string.h>

#include "vmath.h"
#include "bu/color.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


/* Allocate a single typed node for the given point type.  Mirrors the
 * (GED-private) _ged_pnts_new_pnt logic locally so libwdb does not need
 * to link against libged.
 */
static void *
pnts_new_pnt(rt_pnt_type t)
{
    void *point = NULL;
    switch (t) {
	case RT_PNT_TYPE_PNT:
	    BU_ALLOC(point, struct pnt);
	    break;
	case RT_PNT_TYPE_COL:
	    BU_ALLOC(point, struct pnt_color);
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_ALLOC(point, struct pnt_scale);
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_ALLOC(point, struct pnt_normal);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_ALLOC(point, struct pnt_color_scale);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_ALLOC(point, struct pnt_color_normal);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_ALLOC(point, struct pnt_scale_normal);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_ALLOC(point, struct pnt_color_scale_normal);
	    break;
	default:
	    break;
    }
    return point;
}


/* Initialize the bu_list head node carried by rt_pnts_internal.point.
 * Mirrors the (GED-private) _ged_pnts_init_head_pnt logic locally.
 */
static void
pnts_init_head_pnt(struct rt_pnts_internal *pnts)
{
    if (!pnts)
	return;
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT:
	    BU_LIST_INIT(&(((struct pnt *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL:
	    BU_LIST_INIT(&(((struct pnt_color *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_LIST_INIT(&(((struct pnt_scale *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_LIST_INIT(&(((struct pnt_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_LIST_INIT(&(((struct pnt_color_scale *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_LIST_INIT(&(((struct pnt_color_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_LIST_INIT(&(((struct pnt_scale_normal *)pnts->point)->l));
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_LIST_INIT(&(((struct pnt_color_scale_normal *)pnts->point)->l));
	    break;
	default:
	    break;
    }
}


/* Append a node to the tail of the point set's list.  Mirrors the
 * (GED-private) _ged_pnts_add logic locally, but inserts at the tail
 * (BU_LIST_INSERT before the head) so that the resulting list preserves
 * the caller's input ordering.
 */
static void
pnts_add(struct rt_pnts_internal *pnts, void *point)
{
    switch (pnts->type) {
	case RT_PNT_TYPE_PNT:
	    BU_LIST_INSERT(&(((struct pnt *)pnts->point)->l), &((struct pnt *)point)->l);
	    break;
	case RT_PNT_TYPE_COL:
	    BU_LIST_INSERT(&(((struct pnt_color *)pnts->point)->l), &((struct pnt_color *)point)->l);
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_LIST_INSERT(&(((struct pnt_scale *)pnts->point)->l), &((struct pnt_scale *)point)->l);
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_LIST_INSERT(&(((struct pnt_normal *)pnts->point)->l), &((struct pnt_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_LIST_INSERT(&(((struct pnt_color_scale *)pnts->point)->l), &((struct pnt_color_scale *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_LIST_INSERT(&(((struct pnt_color_normal *)pnts->point)->l), &((struct pnt_color_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_LIST_INSERT(&(((struct pnt_scale_normal *)pnts->point)->l), &((struct pnt_scale_normal *)point)->l);
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_LIST_INSERT(&(((struct pnt_color_scale_normal *)pnts->point)->l), &((struct pnt_color_scale_normal *)point)->l);
	    break;
	default:
	    break;
    }
}


/* Returns which optional attributes a given point type carries. */
static int
pnts_has_color(rt_pnt_type type)
{
    return (type == RT_PNT_TYPE_COL ||
	    type == RT_PNT_TYPE_COL_SCA ||
	    type == RT_PNT_TYPE_COL_NRM ||
	    type == RT_PNT_TYPE_COL_SCA_NRM);
}

static int
pnts_has_scale(rt_pnt_type type)
{
    return (type == RT_PNT_TYPE_SCA ||
	    type == RT_PNT_TYPE_COL_SCA ||
	    type == RT_PNT_TYPE_SCA_NRM ||
	    type == RT_PNT_TYPE_COL_SCA_NRM);
}

static int
pnts_has_normal(rt_pnt_type type)
{
    return (type == RT_PNT_TYPE_NRM ||
	    type == RT_PNT_TYPE_COL_NRM ||
	    type == RT_PNT_TYPE_SCA_NRM ||
	    type == RT_PNT_TYPE_COL_SCA_NRM);
}


int
mk_pnts(struct rt_wdb *fp, const char *name, rt_pnt_type type,
	double scale, size_t count, const fastf_t *verts,
	const unsigned char *colors, const fastf_t *scales,
	const fastf_t *normals)
{
    struct rt_pnts_internal *pnts;
    size_t i;
    int want_color;
    int want_scale;
    int want_normal;

    if (!fp || !name)
	return -1;

    if (type == RT_PNT_UNKNOWN || type >= RT_PNT_UNKNOWN)
	return -1;

    /* points are always required */
    if (count && !verts)
	return -1;

    want_color = pnts_has_color(type);
    want_scale = pnts_has_scale(type);
    want_normal = pnts_has_normal(type);

    /* the supplied optional arrays must match exactly what the type
     * implies - no more, no less.
     */
    if (want_color != (colors != NULL))
	return -1;
    if (want_scale != (scales != NULL))
	return -1;
    if (want_normal != (normals != NULL))
	return -1;

    BU_ALLOC(pnts, struct rt_pnts_internal);
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = scale;
    pnts->type = type;
    pnts->count = (unsigned long)count;

    /* allocate and initialize the list head node */
    pnts->point = pnts_new_pnt(type);
    if (!pnts->point) {
	bu_free(pnts, "rt_pnts_internal");
	return -1;
    }
    pnts_init_head_pnt(pnts);

    for (i = 0; i < count; i++) {
	void *point = pnts_new_pnt(type);
	const fastf_t *v = &verts[i*3];

	switch (type) {
	    case RT_PNT_TYPE_PNT:
		{
		    struct pnt *p = (struct pnt *)point;
		    VMOVE(p->v, v);
		}
		break;
	    case RT_PNT_TYPE_COL:
		{
		    struct pnt_color *p = (struct pnt_color *)point;
		    VMOVE(p->v, v);
		    bu_color_from_rgb_chars(&p->c, &colors[i*3]);
		}
		break;
	    case RT_PNT_TYPE_SCA:
		{
		    struct pnt_scale *p = (struct pnt_scale *)point;
		    VMOVE(p->v, v);
		    p->s = scales[i];
		}
		break;
	    case RT_PNT_TYPE_NRM:
		{
		    struct pnt_normal *p = (struct pnt_normal *)point;
		    VMOVE(p->v, v);
		    VMOVE(p->n, &normals[i*3]);
		}
		break;
	    case RT_PNT_TYPE_COL_SCA:
		{
		    struct pnt_color_scale *p = (struct pnt_color_scale *)point;
		    VMOVE(p->v, v);
		    bu_color_from_rgb_chars(&p->c, &colors[i*3]);
		    p->s = scales[i];
		}
		break;
	    case RT_PNT_TYPE_COL_NRM:
		{
		    struct pnt_color_normal *p = (struct pnt_color_normal *)point;
		    VMOVE(p->v, v);
		    bu_color_from_rgb_chars(&p->c, &colors[i*3]);
		    VMOVE(p->n, &normals[i*3]);
		}
		break;
	    case RT_PNT_TYPE_SCA_NRM:
		{
		    struct pnt_scale_normal *p = (struct pnt_scale_normal *)point;
		    VMOVE(p->v, v);
		    p->s = scales[i];
		    VMOVE(p->n, &normals[i*3]);
		}
		break;
	    case RT_PNT_TYPE_COL_SCA_NRM:
		{
		    struct pnt_color_scale_normal *p = (struct pnt_color_scale_normal *)point;
		    VMOVE(p->v, v);
		    bu_color_from_rgb_chars(&p->c, &colors[i*3]);
		    p->s = scales[i];
		    VMOVE(p->n, &normals[i*3]);
		}
		break;
	    default:
		break;
	}

	pnts_add(pnts, point);
    }

    return wdb_export(fp, name, (void *)pnts, ID_PNTS, mk_conv2mm);
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
