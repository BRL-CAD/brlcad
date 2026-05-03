/*                    E D D A T U M 2 . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file primitives/datum/eddatum2.c
 *
 * Datum primitive editing via the rt_edit descriptor framework.
 *
 * A datum is a reference entity that may be a POINT, LINE, or PLANE:
 *
 *   POINT  — only pnt is meaningful.
 *   LINE   — pnt + dir (MAGNITUDE(dir) > 0, w == 0).
 *   PLANE  — pnt + dir + w (!ZERO(w)).
 *
 * Supported operations:
 *   ECMD_DATUM_SET_TYPE   — change the datum type with silent zeroing of
 *                           lower-order fields when order is reduced:
 *                             PLANE → LINE : zero w
 *                             PLANE → POINT: zero w and dir
 *                             LINE  → POINT: zero dir
 *                           Raising order zero-initialises the new fields.
 *   ECMD_DATUM_SET_PNT    — set datum point position.
 *   ECMD_DATUM_SET_DIR    — set datum direction vector (LINE/PLANE only).
 *   ECMD_DATUM_SET_W      — set the w scale factor (PLANE only).
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../edit_private.h"

/*
 * ID_DATUM == 44, so use the 44000 block.
 */
#define ECMD_DATUM_SET_TYPE  44010  /* change datum type (0=POINT,1=LINE,2=PLANE) */
#define ECMD_DATUM_SET_PNT   44011  /* set datum point (x,y,z) */
#define ECMD_DATUM_SET_DIR   44012  /* set datum direction (nx,ny,nz) */
#define ECMD_DATUM_SET_W     44013  /* set datum w scale factor */

/* Enum mirrors the type characterisation in geom.h comments */
#define DATUM_TYPE_POINT 0
#define DATUM_TYPE_LINE  1
#define DATUM_TYPE_PLANE 2

static int
_datum_current_type(const struct rt_datum_internal *dp)
{
    if (!ZERO(dp->w))
	return DATUM_TYPE_PLANE;
    if (MAGNITUDE(dp->dir) > SMALL_FASTF)
	return DATUM_TYPE_LINE;
    return DATUM_TYPE_POINT;
}


/* ------------------------------------------------------------------ *
 * Operation handlers
 * ------------------------------------------------------------------ */

static void
ecmd_datum_set_type(struct rt_edit *s)
{
    struct rt_datum_internal *dp =
	(struct rt_datum_internal *)s->es_int.idb_ptr;
    RT_DATUM_CK_MAGIC(dp);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str,
		"ECMD_DATUM_SET_TYPE: type (0=point,1=line,2=plane) required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    int new_type = (int)s->e_para[0];
    if (new_type < DATUM_TYPE_POINT || new_type > DATUM_TYPE_PLANE) {
	bu_vls_printf(s->log_str,
		"ECMD_DATUM_SET_TYPE: type must be 0, 1, or 2\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    int old_type = _datum_current_type(dp);

    /* Apply silent zeroing when reducing order */
    if (new_type < old_type) {
	if (old_type == DATUM_TYPE_PLANE && new_type == DATUM_TYPE_LINE) {
	    dp->w = 0.0;
	} else if (new_type == DATUM_TYPE_POINT) {
	    dp->w = 0.0;
	    VSETALL(dp->dir, 0.0);
	}
    }
    /* Raising order: new fields are already zero by virtue of the struct;
     * user sets them with subsequent set_dir / set_w ops. */
}

static void
ecmd_datum_set_pnt(struct rt_edit *s)
{
    struct rt_datum_internal *dp =
	(struct rt_datum_internal *)s->es_int.idb_ptr;
    RT_DATUM_CK_MAGIC(dp);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_DATUM_SET_PNT: x y z required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    VSET(dp->pnt,
	 s->e_para[0] * s->local2base,
	 s->e_para[1] * s->local2base,
	 s->e_para[2] * s->local2base);
}

static void
ecmd_datum_set_dir(struct rt_edit *s)
{
    struct rt_datum_internal *dp =
	(struct rt_datum_internal *)s->es_int.idb_ptr;
    RT_DATUM_CK_MAGIC(dp);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_DATUM_SET_DIR: nx ny nz required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    VSET(dp->dir,
	 s->e_para[0] * s->local2base,
	 s->e_para[1] * s->local2base,
	 s->e_para[2] * s->local2base);
}

static void
ecmd_datum_set_w(struct rt_edit *s)
{
    struct rt_datum_internal *dp =
	(struct rt_datum_internal *)s->es_int.idb_ptr;
    RT_DATUM_CK_MAGIC(dp);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str,
		"ECMD_DATUM_SET_W: w required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    dp->w = s->e_para[0] * s->local2base;
}


/* ================================================================== *
 * Public interface                                                    *
 * ================================================================== */

/* No per-edit state needed for datum (no selection). */
void *
rt_edit_datum_prim_edit_create(struct rt_edit *UNUSED(s))
{
    return NULL;
}

void
rt_edit_datum_prim_edit_destroy(void *UNUSED(ptr))
{
}

void
rt_edit_datum_prim_edit_reset(struct rt_edit *UNUSED(s))
{
}

void
rt_edit_datum_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);
    s->edit_mode = RT_PARAMS_EDIT_TRANS;

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f) (*f)(0, NULL, d, &flag);
}

int
rt_edit_datum_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    edit_srot(s);
	    break;
	case ECMD_DATUM_SET_TYPE:
	    ecmd_datum_set_type(s);
	    break;
	case ECMD_DATUM_SET_PNT:
	    ecmd_datum_set_pnt(s);
	    break;
	case ECMD_DATUM_SET_DIR:
	    ecmd_datum_set_dir(s);
	    break;
	case ECMD_DATUM_SET_W:
	    ecmd_datum_set_w(s);
	    break;
	default:
	    return edit_generic(s);
    }
    return 0;
}

int
rt_edit_datum_edit_xy(struct rt_edit *s, vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	default:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
    }
    edit_abs_tra(s, pos_view);
    return 0;
}


/* ------------------------------------------------------------------ *
 * Descriptor
 * ------------------------------------------------------------------ */

/* Type enum: 0=POINT, 1=LINE, 2=PLANE */
static const char *datum_type_enum_names[] = { "point", "line", "plane", NULL };
static const int   datum_type_enum_vals[]  = { 0, 1, 2 };

static const struct rt_edit_param_desc datum_type_param[] = {
    { "type",
      "Type (0=point 1=line 2=plane; lowering order zeroes removed fields)",
      RT_EDIT_PARAM_SCALAR, 0,
      0.0, 2.0, "enum",
      3, datum_type_enum_names, datum_type_enum_vals,
      NULL }
};

static const struct rt_edit_param_desc datum_pnt_param[] = {
    { "pnt", "Position", RT_EDIT_PARAM_POINT, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc datum_dir_param[] = {
    { "dir", "Direction", RT_EDIT_PARAM_VECTOR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc datum_w_param[] = {
    { "w", "Scale factor (PLANE only; set to non-zero to make datum a plane)",
      RT_EDIT_PARAM_SCALAR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_cmd_desc datum_cmds[] = {
    { ECMD_DATUM_SET_TYPE, "Set Type",      "datum", 1, datum_type_param, 1, 10 },
    { ECMD_DATUM_SET_PNT,  "Set Point",     "datum", 1, datum_pnt_param,  1, 20 },
    { ECMD_DATUM_SET_DIR,  "Set Direction", "datum", 1, datum_dir_param,  1, 30 },
    { ECMD_DATUM_SET_W,    "Set W",         "datum", 1, datum_w_param,    1, 40 }
};

static const struct rt_edit_prim_desc datum_prim_desc = {
    "datum", "Datum", 4, datum_cmds
};

const struct rt_edit_prim_desc *
rt_edit_datum_edit_desc(void)
{
    return &datum_prim_desc;
}


int
rt_edit_datum_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    if (!s || !vals) return 0;

    struct rt_datum_internal *dp =
	(struct rt_datum_internal *)s->es_int.idb_ptr;

    switch (cmd_id) {
	case ECMD_DATUM_SET_TYPE: {
	    int t = _datum_current_type(dp);
	    vals[0] = (fastf_t)t;
	    return 1;
	}

	case ECMD_DATUM_SET_PNT:
	    vals[0] = dp->pnt[0] * s->base2local;
	    vals[1] = dp->pnt[1] * s->base2local;
	    vals[2] = dp->pnt[2] * s->base2local;
	    return 3;

	case ECMD_DATUM_SET_DIR:
	    vals[0] = dp->dir[0] * s->base2local;
	    vals[1] = dp->dir[1] * s->base2local;
	    vals[2] = dp->dir[2] * s->base2local;
	    return 3;

	case ECMD_DATUM_SET_W:
	    vals[0] = dp->w * s->base2local;
	    return 1;

	default:
	    return 0;
    }
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
