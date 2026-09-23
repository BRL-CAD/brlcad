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

static int
revolve_set_length_vector(struct rt_edit *s, fastf_t *dest, const char *operation)
{
    if (s->e_inpara != 3) {
	bu_vls_printf(s->log_str, "%s: x y z required\n", operation);
	return BRLCAD_ERROR;
    }

    VSCALE(dest, s->e_para, s->local2base);
    return BRLCAD_OK;
}

static int
ecmd_revolve_set_ang(struct rt_edit *s)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    if (s->e_inpara != 1) {
	bu_vls_printf(s->log_str,
		"ECMD_REVOLVE_SET_ANG: angle (degrees) required\n");
	return BRLCAD_ERROR;
    }

    rip->ang = s->e_para[0] * DEG2RAD;
    return BRLCAD_OK;
}

static int
ecmd_revolve_set_skt(struct rt_edit *s)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

    if (!s->e_nstr || !s->e_str[0][0] || !s->dbip) {
	s->e_nstr = 0;
	bu_vls_printf(s->log_str,
		"ECMD_REVOLVE_SET_SKT: sketch name and database required\n");
	return BRLCAD_ERROR;
    }

    const char *name = s->e_str[0];
    s->e_nstr = 0;
    struct directory *dp = db_lookup(s->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(s->log_str, "ECMD_REVOLVE_SET_SKT: %s does not exist\n", name);
	return BRLCAD_ERROR;
    }

    struct rt_db_internal new_ip;
    RT_DB_INTERNAL_INIT(&new_ip);
    int id = rt_db_get_internal(&new_ip, dp, s->dbip, bn_mat_identity);
    if (id != ID_SKETCH) {
	if (id > 0)
	    rt_db_free_internal(&new_ip);
	bu_vls_printf(s->log_str, "ECMD_REVOLVE_SET_SKT: %s is not a sketch\n", name);
	return BRLCAD_ERROR;
    }

    if (rip->skt) {
	struct rt_db_internal old_ip;
	RT_DB_INTERNAL_INIT(&old_ip);
	old_ip.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	old_ip.idb_type = ID_SKETCH;
	old_ip.idb_ptr = rip->skt;
	old_ip.idb_meth = &OBJ[ID_SKETCH];
	rt_db_free_internal(&old_ip);
    }

    bu_vls_strcpy(&rip->sketch_name, name);
    rip->skt = (struct rt_sketch_internal *)new_ip.idb_ptr;
    return BRLCAD_OK;
}


/* ================================================================== *
 * Public interface                                                    *
 * ================================================================== */

C_DECL void *
rt_edit_revolve_prim_edit_create(struct rt_edit *UNUSED(s))
{
    return NULL;
}

C_DECL void
rt_edit_revolve_prim_edit_destroy(void *UNUSED(ptr))
{
}

C_DECL void
rt_edit_revolve_prim_edit_reset(struct rt_edit *UNUSED(s))
{
}

C_DECL void
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

C_DECL int
rt_edit_revolve_edit(struct rt_edit *s)
{
    struct rt_revolve_internal *rip =
	(struct rt_revolve_internal *)s->es_int.idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);

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
	    return revolve_set_length_vector(s, rip->v3d, "ECMD_REVOLVE_SET_V");
	case ECMD_REVOLVE_SET_AXIS:
	    return revolve_set_length_vector(s, rip->axis3d, "ECMD_REVOLVE_SET_AXIS");
	case ECMD_REVOLVE_SET_R:
	    return revolve_set_length_vector(s, rip->r, "ECMD_REVOLVE_SET_R");
	case ECMD_REVOLVE_SET_ANG:
	    return ecmd_revolve_set_ang(s);
	case ECMD_REVOLVE_SET_SKT:
	    return ecmd_revolve_set_skt(s);
	default:
	    return edit_generic(s);
    }
    return BRLCAD_OK;
}

C_DECL int
rt_edit_revolve_edit_xy(struct rt_edit *s, const vect_t mousevec)
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
    { ECMD_REVOLVE_SET_V,    "Set Vertex",       "geometry", 1, revolve_v_param,    1, 10, NULL },
    { ECMD_REVOLVE_SET_AXIS, "Set Axis",         "geometry", 1, revolve_axis_param, 1, 20, NULL },
    { ECMD_REVOLVE_SET_R,    "Set Start Vector", "geometry", 1, revolve_r_param,    1, 30, NULL },
    { ECMD_REVOLVE_SET_ANG,  "Set Sweep Angle",  "geometry", 1, revolve_ang_param,  1, 40, NULL },
    { ECMD_REVOLVE_SET_SKT,  "Set Sketch Name",  "geometry", 1, revolve_skt_param,  1, 50, NULL }
};

static const struct rt_edit_prim_desc revolve_prim_desc = {
    "revolve", "Revolve", 5, revolve_cmds,
    0,                    /* nopt         */
    NULL                  /* opts         */
};

C_DECL const struct rt_edit_prim_desc *
rt_edit_revolve_edit_desc(void)
{
    return &revolve_prim_desc;
}


C_DECL int
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
