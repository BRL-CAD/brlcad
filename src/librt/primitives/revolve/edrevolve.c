/*                    E D R E V O L V E . C
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
/** @file primitives/revolve/edrevolve.c
 *
 * Solid-of-Revolution (REVOLVE) primitive editing via the rt_edit framework.
 *
 * struct rt_revolve_internal fields:
 *   v3d    — vertex in 3-D space.
 *   axis3d — revolve axis in 3-D space.
 *   r      — vector in the start plane (perpendicular to axis3d).
 *   ang    — sweep angle in radians.
 *   sketch_name — name of the driving sketch object.
 *
 * Supported operations:
 *   ECMD_REVOLVE_SET_V    — set vertex position v3d.
 *   ECMD_REVOLVE_SET_AXIS — set axis direction axis3d.
 *   ECMD_REVOLVE_SET_R    — set start-plane vector r.
 *   ECMD_REVOLVE_SET_ANG  — set sweep angle (degrees in, stored as radians).
 *   ECMD_REVOLVE_SET_SKT  — set sketch name.
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
 * ID_REVOLVE == 40, so use the 40000 block.
 */
#define ECMD_REVOLVE_SET_V    40010  /* set vertex v3d */
#define ECMD_REVOLVE_SET_AXIS 40011  /* set axis direction axis3d */
#define ECMD_REVOLVE_SET_R    40012  /* set start-plane vector r */
#define ECMD_REVOLVE_SET_ANG  40013  /* set sweep angle (degrees) */
#define ECMD_REVOLVE_SET_SKT  40014  /* set sketch name */


/* ------------------------------------------------------------------ *
 * Operation handlers
 * ------------------------------------------------------------------ */

static void
ecmd_revolve_set_v(struct rt_edit *s)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_REVOLVE_SET_V: x y z required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    VSET(rip->v3d,
	 s->e_para[0] * s->local2base,
	 s->e_para[1] * s->local2base,
	 s->e_para[2] * s->local2base);
}

static void
ecmd_revolve_set_axis(struct rt_edit *s)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_REVOLVE_SET_AXIS: nx ny nz required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    VSET(rip->axis3d,
	 s->e_para[0] * s->local2base,
	 s->e_para[1] * s->local2base,
	 s->e_para[2] * s->local2base);
}

static void
ecmd_revolve_set_r(struct rt_edit *s)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str,
		"ECMD_REVOLVE_SET_R: rx ry rz required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    VSET(rip->r,
	 s->e_para[0] * s->local2base,
	 s->e_para[1] * s->local2base,
	 s->e_para[2] * s->local2base);
}

static void
ecmd_revolve_set_ang(struct rt_edit *s)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str,
		"ECMD_REVOLVE_SET_ANG: angle (degrees) required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    rip->ang = s->e_para[0] * DEG2RAD;
}

static void
ecmd_revolve_set_skt(struct rt_edit *s)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);
    bu_clbk_t f = NULL;
    void *d = NULL;

    const char *skt = (s->e_nstr > 0 && s->e_str[0][0]) ? s->e_str[0] : NULL;
    if (!skt) {
	bu_vls_printf(s->log_str,
		"ECMD_REVOLVE_SET_SKT: sketch name required\n");
	rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
	if (f) (*f)(0, NULL, d, NULL);
	return;
    }

    bu_vls_trunc(&rip->sketch_name, 0);
    bu_vls_strcat(&rip->sketch_name, skt);
}


/* ================================================================== *
 * Public interface                                                    *
 * ================================================================== */

void *
rt_edit_revolve_prim_edit_create(struct rt_edit *UNUSED(s))
{
    return NULL;
}

void
rt_edit_revolve_prim_edit_destroy(void *UNUSED(ptr))
{
}

void
rt_edit_revolve_prim_edit_reset(struct rt_edit *UNUSED(s))
{
}

void
rt_edit_revolve_set_edit_mode(struct rt_edit *s, int mode)
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
rt_edit_revolve_edit(struct rt_edit *s)
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
	case ECMD_REVOLVE_SET_V:
	    ecmd_revolve_set_v(s);
	    break;
	case ECMD_REVOLVE_SET_AXIS:
	    ecmd_revolve_set_axis(s);
	    break;
	case ECMD_REVOLVE_SET_R:
	    ecmd_revolve_set_r(s);
	    break;
	case ECMD_REVOLVE_SET_ANG:
	    ecmd_revolve_set_ang(s);
	    break;
	case ECMD_REVOLVE_SET_SKT:
	    ecmd_revolve_set_skt(s);
	    break;
	default:
	    return edit_generic(s);
    }
    return 0;
}

int
rt_edit_revolve_edit_xy(struct rt_edit *s, vect_t mousevec)
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

static const struct rt_edit_param_desc revolve_v_param[] = {
    { "v", "Vertex position", RT_EDIT_PARAM_POINT, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc revolve_axis_param[] = {
    { "axis", "Axis direction", RT_EDIT_PARAM_VECTOR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc revolve_r_param[] = {
    { "r", "Start-plane vector", RT_EDIT_PARAM_VECTOR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "length",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc revolve_ang_param[] = {
    { "ang", "Sweep angle", RT_EDIT_PARAM_SCALAR, 0,
      RT_EDIT_PARAM_NO_LIMIT, RT_EDIT_PARAM_NO_LIMIT, "degrees",
      0, NULL, NULL, NULL }
};

static const struct rt_edit_param_desc revolve_skt_param[] = {
    { "sketch_name", "Sketch name", RT_EDIT_PARAM_STRING, 0,
      0.0, 0.0, "", 0, NULL, NULL, NULL }
};

static const struct rt_edit_cmd_desc revolve_cmds[] = {
    { ECMD_REVOLVE_SET_V,    "Set Vertex",       "geometry", 1, revolve_v_param,    1, 10 },
    { ECMD_REVOLVE_SET_AXIS, "Set Axis",         "geometry", 1, revolve_axis_param, 1, 20 },
    { ECMD_REVOLVE_SET_R,    "Set Start Vector", "geometry", 1, revolve_r_param,    1, 30 },
    { ECMD_REVOLVE_SET_ANG,  "Set Sweep Angle",  "geometry", 1, revolve_ang_param,  1, 40 },
    { ECMD_REVOLVE_SET_SKT,  "Set Sketch Name",  "geometry", 1, revolve_skt_param,  1, 50 }
};

static const struct rt_edit_prim_desc revolve_prim_desc = {
    "revolve", "Revolve", 5, revolve_cmds
};

const struct rt_edit_prim_desc *
rt_edit_revolve_edit_desc(void)
{
    return &revolve_prim_desc;
}


int
rt_edit_revolve_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    if (!s || !vals) return 0;

    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;

    switch (cmd_id) {
	case ECMD_REVOLVE_SET_V:
	    vals[0] = rip->v3d[0] * s->base2local;
	    vals[1] = rip->v3d[1] * s->base2local;
	    vals[2] = rip->v3d[2] * s->base2local;
	    return 3;

	case ECMD_REVOLVE_SET_AXIS:
	    vals[0] = rip->axis3d[0] * s->base2local;
	    vals[1] = rip->axis3d[1] * s->base2local;
	    vals[2] = rip->axis3d[2] * s->base2local;
	    return 3;

	case ECMD_REVOLVE_SET_R:
	    vals[0] = rip->r[0] * s->base2local;
	    vals[1] = rip->r[1] * s->base2local;
	    vals[2] = rip->r[2] * s->base2local;
	    return 3;

	case ECMD_REVOLVE_SET_ANG:
	    vals[0] = rip->ang * RAD2DEG;
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
