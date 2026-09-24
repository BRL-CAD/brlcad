/*                         E D P A R T . C
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
/** @file primitives/edpart.c
 *
 */

#include "common.h"

#include "vmath.h"
#include "bu/str.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../edit_private.h"
#include "part_private.h"

#define ECMD_PART_H		16088
#define ECMD_PART_VRAD		16089
#define ECMD_PART_HRAD		16090

C_DECL void
rt_edit_part_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_PART_H:
	case ECMD_PART_VRAD:
	case ECMD_PART_HRAD:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	default:
	    break;
    };

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}

static void
part_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_part_set_edit_mode(s, arg);
}

struct rt_edit_menu_item part_menu[] = {
    { "Particle MENU", NULL, 0 },
    { "Set H", part_ed, ECMD_PART_H },
    { "Set v", part_ed, ECMD_PART_VRAD },
    { "Set h", part_ed, ECMD_PART_HRAD },
    { "", NULL, 0 }
};

C_DECL struct rt_edit_menu_item *
rt_edit_part_menu_item(const struct bn_tol *UNUSED(tol))
{
    return part_menu;
}

/* ft_edit_desc descriptor for the Particle primitive                */
/* ------------------------------------------------------------------ */

static const struct rt_edit_param_desc part_h_params[] = {
    {
	"h",                  /* name         */
	"Height (magnitude)", /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc part_vrad_params[] = {
    {
	"vrad",               /* name         */
	"V-End Radius",       /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc part_hrad_params[] = {
    {
	"hrad",               /* name         */
	"H-End Radius",       /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc part_cmds[] = {
    {
	ECMD_PART_H,          /* cmd_id       */
	"Set H",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	part_h_params,        /* params       */
	1,                    /* interactive  */
	10                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PART_VRAD,       /* cmd_id       */
	"Set v radius",       /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	part_vrad_params,     /* params       */
	1,                    /* interactive  */
	20                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_PART_HRAD,       /* cmd_id       */
	"Set h radius",       /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	part_hrad_params,     /* params       */
	1,                    /* interactive  */
	30                    /* display_order */,
	NULL                  /* req_types */
    }
};

static const struct rt_edit_prim_desc part_prim_desc = {
    "part",               /* prim_type    */
    "Particle",           /* prim_label   */
    3,                    /* ncmd         */
    part_cmds             /* cmds         */,
    0,                    /* nopt         */
    NULL                  /* opts         */
};

C_DECL const struct rt_edit_prim_desc *
rt_edit_part_edit_desc(void)
{
    return &part_prim_desc;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

C_DECL void
rt_edit_part_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_part_internal *part = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(part);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(part->part_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(part->part_H));
    bu_vls_printf(p, "v radius: %.9f\n", part->part_vrad * base2local);
    bu_vls_printf(p, "h radius: %.9f\n", part->part_hrad * base2local);
}

C_DECL int
rt_edit_part_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    struct rt_part_internal *part = (struct rt_part_internal *)ip->idb_ptr;
    RT_PART_CK_MAGIC(part);

    if (!fc)
	return BRLCAD_ERROR;

    struct rt_part_internal staged = *part;
    char *buffer = bu_strdup(fc);
    char *cursor = buffer;
    int result = BRLCAD_ERROR;
    if (edit_param_read_vector(staged.part_V, &cursor, "Vertex", local2base) != BRLCAD_OK ||
	edit_param_read_vector(staged.part_H, &cursor, "Height", local2base) != BRLCAD_OK ||
	edit_param_read_scalar(&staged.part_vrad, &cursor, "v radius", local2base) != BRLCAD_OK ||
	edit_param_read_scalar(&staged.part_hrad, &cursor, "h radius", local2base) != BRLCAD_OK ||
	edit_param_next_line(&cursor) ||
	part_update_type(&staged) != BRLCAD_OK)
	goto cleanup;

    *part = staged;
    result = BRLCAD_OK;

cleanup:
    bu_free(buffer, "PART parameter text");
    return result;
}

static int
rt_edit_part_pscale(struct rt_edit *s)
{
    struct rt_part_internal *part = (struct rt_part_internal *)s->es_int.idb_ptr;
    RT_PART_CK_MAGIC(part);

    vect_t *axis = NULL;
    fastf_t *radius = NULL;
    switch (s->edit_flag) {
	case ECMD_PART_H:
	    axis = &part->part_H;
	    break;
	case ECMD_PART_VRAD:
	    radius = &part->part_vrad;
	    break;
	case ECMD_PART_HRAD:
	    radius = &part->part_hrad;
	    break;
	default:
	    return BRLCAD_ERROR;
    }
    return edit_scale_length(s, axis, radius);
}

C_DECL int
rt_edit_part_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case ECMD_PART_H:
	case ECMD_PART_VRAD:
	case ECMD_PART_HRAD:
	    return rt_edit_part_pscale(s);
	default:
	    return edit_generic(s);
    }
}

C_DECL int
rt_edit_part_edit_xy(
        struct rt_edit *s,
        const vect_t mousevec
        )
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */

    switch (s->edit_flag) {
        case RT_PARAMS_EDIT_SCALE:
	case ECMD_PART_H:
	case ECMD_PART_VRAD:
	case ECMD_PART_HRAD:
	    edit_sscale_xy(s, mousevec);
            return 0;
        case RT_PARAMS_EDIT_TRANS:
            edit_stra_xy(&pos_view, s, mousevec);
            edit_abs_tra(s, pos_view);
            return 0;
        default:
            return edit_generic_xy(s, mousevec);
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
