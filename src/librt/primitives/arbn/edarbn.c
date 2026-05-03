/*                       E D A R B N . C
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
/** @file primitives/arbn/edarbn.c
 *
 * Arbitrary N-hedron (ARBN) primitive editing via the rt_edit framework.
 *
 * Supported operations:
 *   ECMD_ARBN_PLANE_SELECT   — select a plane by index
 *   ECMD_ARBN_PLANE_SET_DIST — move the selected plane by setting its
 *                              signed distance from the origin (the fourth
 *                              coefficient of the plane equation).
 *   ECMD_ARBN_PLANE_SET_NORM — replace the unit normal of the selected plane.
 *   ECMD_ARBN_PLANE_ROTATE   — rotate the normal of the selected plane by the
 *                              given Euler angles (rx, ry, rz in degrees).
 *   ECMD_ARBN_PLANE_ADD      — append a new bounding half-space to eqn[].
 *   ECMD_ARBN_PLANE_DEL      — remove the selected plane from eqn[].
 *
 * After any structural change the caller should re-prep the solid to verify
 * convexity (arbn prep checks for unused planes).
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../edit_private.h"

/*
 * ID_ARBN == 14, so use the 14000 block.
 */
#define ECMD_ARBN_PLANE_SELECT    14010   /* select a plane by index */
#define ECMD_ARBN_PLANE_SET_DIST  14011   /* set d (4th coeff) of selected plane */
#define ECMD_ARBN_PLANE_SET_NORM  14012   /* set unit normal of selected plane */
#define ECMD_ARBN_PLANE_ROTATE    14013   /* rotate normal of selected plane (rx,ry,rz deg) */
#define ECMD_ARBN_PLANE_ADD       14014   /* append a new half-space */
#define ECMD_ARBN_PLANE_DEL       14015   /* delete selected plane */

/* Selection state */
struct rt_arbn_edit {
    int plane_index;   /* selected plane index (-1 = none) */
};


/* ------------------------------------------------------------------ *
 * Internal helpers
 * ------------------------------------------------------------------ */

/* Clamp the plane normal to unit length; return -1 if zero normal. */
static int
_arbn_normalize_plane(plane_t eqn)
{
    double mag = MAGNITUDE(eqn);
    if (ZERO(mag)) return -1;
    eqn[0] /= mag;
    eqn[1] /= mag;
    eqn[2] /= mag;
    eqn[3] /= mag;
    return 0;
}

static void
ecmd_arbn_plane_select(struct rt_edit *s)
{
    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;
    RT_ARBN_CK_MAGIC(aip);
    struct rt_arbn_edit *e = (struct rt_arbn_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_SELECT: plane_index required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    int idx = (int)s->e_para[0];
    if (idx < 0 || (size_t)idx >= aip->neqn) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_SELECT: index %d out of range [0,%zu)\n",
		idx, aip->neqn);
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    e->plane_index = idx;

    bu_vls_printf(s->log_str,
	    "Selected arbn plane %d: N=(%.6f,%.6f,%.6f) d=%.6f\n",
	    idx,
	    aip->eqn[idx][0], aip->eqn[idx][1], aip->eqn[idx][2],
	    aip->eqn[idx][3]);
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
    if (f) (*f)(0, NULL, d, NULL);
}

static void
ecmd_arbn_plane_set_dist(struct rt_edit *s)
{
    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;
    RT_ARBN_CK_MAGIC(aip);
    struct rt_arbn_edit *e = (struct rt_arbn_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (e->plane_index < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_SET_DIST: no plane selected\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }
    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_SET_DIST: distance required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    aip->eqn[e->plane_index][3] = s->e_para[0] * s->local2base;
}

static void
ecmd_arbn_plane_set_norm(struct rt_edit *s)
{
    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;
    RT_ARBN_CK_MAGIC(aip);
    struct rt_arbn_edit *e = (struct rt_arbn_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (e->plane_index < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_SET_NORM: no plane selected\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_SET_NORM: nx ny nz required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    plane_t *eq = &aip->eqn[e->plane_index];
    (*eq)[0] = s->e_para[0];
    (*eq)[1] = s->e_para[1];
    (*eq)[2] = s->e_para[2];

    if (_arbn_normalize_plane(*eq) < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_SET_NORM: zero-length normal rejected\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
    }
}

static void
ecmd_arbn_plane_rotate(struct rt_edit *s)
{
    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;
    RT_ARBN_CK_MAGIC(aip);
    struct rt_arbn_edit *e = (struct rt_arbn_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (e->plane_index < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_ROTATE: no plane selected\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_ROTATE: rx ry rz (degrees) required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    mat_t rmat;
    bn_mat_angles(rmat, s->e_para[0], s->e_para[1], s->e_para[2]);

    plane_t *eq = &aip->eqn[e->plane_index];
    vect_t old_norm, new_norm;
    VSET(old_norm, (*eq)[0], (*eq)[1], (*eq)[2]);
    MAT4X3VEC(new_norm, rmat, old_norm);
    VUNITIZE(new_norm);
    (*eq)[0] = new_norm[0];
    (*eq)[1] = new_norm[1];
    (*eq)[2] = new_norm[2];
}

static void
ecmd_arbn_plane_add(struct rt_edit *s)
{
    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;
    RT_ARBN_CK_MAGIC(aip);
    struct rt_arbn_edit *e = (struct rt_arbn_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 4) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_ADD: nx ny nz d required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    size_t new_n = aip->neqn + 1;
    aip->eqn = (plane_t *)bu_realloc(aip->eqn,
	    new_n * sizeof(plane_t), "arbn eqn[] grow");
    aip->eqn[aip->neqn][0] = s->e_para[0];
    aip->eqn[aip->neqn][1] = s->e_para[1];
    aip->eqn[aip->neqn][2] = s->e_para[2];
    aip->eqn[aip->neqn][3] = s->e_para[3] * s->local2base;

    if (_arbn_normalize_plane(aip->eqn[aip->neqn]) < 0) {
	/* roll back */
	aip->eqn = (plane_t *)bu_realloc(aip->eqn,
		aip->neqn * sizeof(plane_t), "arbn eqn[] rollback");
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_ADD: zero-length normal rejected\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    e->plane_index = (int)aip->neqn;   /* select newly added plane */
    aip->neqn = new_n;
}

static void
ecmd_arbn_plane_del(struct rt_edit *s)
{
    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;
    RT_ARBN_CK_MAGIC(aip);
    struct rt_arbn_edit *e = (struct rt_arbn_edit *)s->ipe_ptr;
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (e->plane_index < 0) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_DEL: no plane selected\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }
    if (aip->neqn <= 4) {
	bu_vls_printf(s->log_str,
		"ECMD_ARBN_PLANE_DEL: cannot reduce below 4 planes\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    int idx = e->plane_index;
    /* Shift remaining planes down. */
    for (size_t i = (size_t)idx + 1; i < aip->neqn; i++)
	HMOVE(aip->eqn[i-1], aip->eqn[i]);
    aip->neqn--;
    aip->eqn = (plane_t *)bu_realloc(aip->eqn,
	    aip->neqn * sizeof(plane_t), "arbn eqn[] shrink");
    e->plane_index = -1;
}


/* ================================================================== *
 * Public interface                                                    *
 * ================================================================== */

void *
rt_edit_arbn_prim_edit_create(struct rt_edit *UNUSED(s))
{
    struct rt_arbn_edit *e;
    BU_GET(e, struct rt_arbn_edit);
    e->plane_index = -1;
    return (void *)e;
}

void
rt_edit_arbn_prim_edit_destroy(void *ptr)
{
    struct rt_arbn_edit *e = (struct rt_arbn_edit *)ptr;
    if (!e) return;
    BU_PUT(e, struct rt_arbn_edit);
}

void
rt_edit_arbn_prim_edit_reset(struct rt_edit *s)
{
    struct rt_arbn_edit *e = (struct rt_arbn_edit *)s->ipe_ptr;
    e->plane_index = -1;
}

void
rt_edit_arbn_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_ARBN_PLANE_SELECT:
	    s->edit_mode = RT_PARAMS_EDIT_PICK;
	    break;
	default:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
    }

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f) (*f)(0, NULL, d, &flag);
}

int
rt_edit_arbn_edit(struct rt_edit *s)
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
	case ECMD_ARBN_PLANE_SELECT:
	    ecmd_arbn_plane_select(s);
	    break;
	case ECMD_ARBN_PLANE_SET_DIST:
	    ecmd_arbn_plane_set_dist(s);
	    break;
	case ECMD_ARBN_PLANE_SET_NORM:
	    ecmd_arbn_plane_set_norm(s);
	    break;
	case ECMD_ARBN_PLANE_ROTATE:
	    ecmd_arbn_plane_rotate(s);
	    break;
	case ECMD_ARBN_PLANE_ADD:
	    ecmd_arbn_plane_add(s);
	    break;
	case ECMD_ARBN_PLANE_DEL:
	    ecmd_arbn_plane_del(s);
	    break;
	default:
	    return edit_generic(s);
    }
    return 0;
}

int
rt_edit_arbn_edit_xy(struct rt_edit *s, vect_t mousevec)
{
    vect_t pos_view = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_ARBN_PLANE_SELECT:
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

static const struct rt_edit_param_desc arbn_index_param[] = {
    { "plane_index", "Plane Index", RT_EDIT_PARAM_SCALAR, 0,
      0.0, RT_EDIT_PARAM_NO_LIMIT, "count", 0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc arbn_dist_param[] = {
    { "d", "Distance", RT_EDIT_PARAM_SCALAR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc arbn_normal_param[] = {
    { "normal", "Unit Normal", RT_EDIT_PARAM_VECTOR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "unitless",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc arbn_angles_param[] = {
    { "angles", "Euler Angles (deg)", RT_EDIT_PARAM_VECTOR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "degrees",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc arbn_plane_param[] = {
    { "nx",  "Normal X",  RT_EDIT_PARAM_SCALAR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "unitless", 0, NULL, NULL, NULL },
    { "ny",  "Normal Y",  RT_EDIT_PARAM_SCALAR, 1,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "unitless", 0, NULL, NULL, NULL },
    { "nz",  "Normal Z",  RT_EDIT_PARAM_SCALAR, 2,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "unitless", 0, NULL, NULL, NULL },
    { "d",   "Distance",  RT_EDIT_PARAM_SCALAR, 3,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",   0, NULL, NULL, NULL }
};

static const struct rt_edit_cmd_desc arbn_cmds[] = {
    { ECMD_ARBN_PLANE_SELECT,   "Select Plane",      "plane", 1, arbn_index_param,  0, 10 },
    { ECMD_ARBN_PLANE_SET_DIST, "Set Plane Distance","plane", 1, arbn_dist_param,   1, 20 },
    { ECMD_ARBN_PLANE_SET_NORM, "Set Plane Normal",  "plane", 1, arbn_normal_param, 1, 30 },
    { ECMD_ARBN_PLANE_ROTATE,   "Rotate Plane Normal","plane",1, arbn_angles_param, 1, 40 },
    { ECMD_ARBN_PLANE_ADD,      "Add Plane",         "plane", 4, arbn_plane_param,  1, 50 },
    { ECMD_ARBN_PLANE_DEL,      "Delete Plane",      "plane", 0, NULL,              0, 60 }
};

static const struct rt_edit_prim_desc arbn_prim_desc = {
    "arbn", "ARBN", 6, arbn_cmds
};

const struct rt_edit_prim_desc *
rt_edit_arbn_edit_desc(void)
{
    return &arbn_prim_desc;
}


int
rt_edit_arbn_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    if (!s || !vals) return 0;

    struct rt_arbn_edit *e = (struct rt_arbn_edit *)s->ipe_ptr;
    struct rt_arbn_internal *aip =
	(struct rt_arbn_internal *)s->es_int.idb_ptr;

    switch (cmd_id) {
	case ECMD_ARBN_PLANE_SELECT:
	    vals[0] = (fastf_t)e->plane_index;
	    return 1;

	case ECMD_ARBN_PLANE_SET_DIST:
	    if (e->plane_index < 0 || (size_t)e->plane_index >= aip->neqn)
		return 0;
	    vals[0] = aip->eqn[e->plane_index][3] * s->base2local;
	    return 1;

	case ECMD_ARBN_PLANE_SET_NORM:
	case ECMD_ARBN_PLANE_ROTATE:
	    if (e->plane_index < 0 || (size_t)e->plane_index >= aip->neqn)
		return 0;
	    vals[0] = aip->eqn[e->plane_index][0];
	    vals[1] = aip->eqn[e->plane_index][1];
	    vals[2] = aip->eqn[e->plane_index][2];
	    return 3;

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
