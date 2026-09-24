/*                         E D R H C . C
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
/** @file primitives/edrhc.c
 *
 */

#include "common.h"

#include "bu/opt.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../edit_private.h"

#define ECMD_RHC_B		18046
#define ECMD_RHC_H		18047
#define ECMD_RHC_R		18048
#define ECMD_RHC_C		18049

C_DECL void
rt_edit_rhc_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_RHC_B:
	case ECMD_RHC_H:
	case ECMD_RHC_R:
	case ECMD_RHC_C:
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
rhc_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_rhc_set_edit_mode(s, arg);
}

struct rt_edit_menu_item rhc_menu[] = {
    { "RHC MENU", NULL, 0 },
    { "Set B", rhc_ed, ECMD_RHC_B },
    { "Set H", rhc_ed, ECMD_RHC_H },
    { "Set r", rhc_ed, ECMD_RHC_R },
    { "Set c", rhc_ed, ECMD_RHC_C },
    { "", NULL, 0 }
};

C_DECL struct rt_edit_menu_item *
rt_edit_rhc_menu_item(const struct bn_tol *UNUSED(tol))
{
    return rhc_menu;
}

/* ft_edit_desc descriptor for the Right Hyperbolic Cylinder primitive */
/* ------------------------------------------------------------------ */

static const struct rt_edit_param_desc rhc_b_params[] = {
    {
	"b",                  /* name         */
	"Half-Width B",       /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc rhc_h_params[] = {
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

static const struct rt_edit_param_desc rhc_r_params[] = {
    {
	"r",                  /* name         */
	"Rectangular Half-Width", /* label    */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc rhc_c_params[] = {
    {
	"c",                  /* name         */
	"Asymptote Constant c", /* label      */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc rhc_cmds[] = {
    {
	ECMD_RHC_B,           /* cmd_id       */
	"Set B",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	rhc_b_params,         /* params       */
	1,                    /* interactive  */
	10                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_RHC_H,           /* cmd_id       */
	"Set H",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	rhc_h_params,         /* params       */
	1,                    /* interactive  */
	20                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_RHC_R,           /* cmd_id       */
	"Set r",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	rhc_r_params,         /* params       */
	1,                    /* interactive  */
	30                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_RHC_C,           /* cmd_id       */
	"Set c",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	rhc_c_params,         /* params       */
	1,                    /* interactive  */
	40                    /* display_order */,
	NULL                  /* req_types */
    }
};

static const struct rt_edit_prim_desc rhc_prim_desc = {
    "rhc",                /* prim_type    */
    "Right Hyperbolic Cylinder", /* prim_label */
    4,                    /* ncmd         */
    rhc_cmds              /* cmds         */,
    0,                    /* nopt         */
    NULL                  /* opts         */
};

C_DECL const struct rt_edit_prim_desc *
rt_edit_rhc_edit_desc(void)
{
    return &rhc_prim_desc;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

C_DECL void
rt_edit_rhc_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_rhc_internal *rhc = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(rhc);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(rhc->rhc_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(rhc->rhc_H));
    bu_vls_printf(p, "Breadth: %.9f %.9f %.9f\n", V3BASE2LOCAL(rhc->rhc_B));
    bu_vls_printf(p, "Half-width: %.9f\n", rhc->rhc_r * base2local);
    bu_vls_printf(p, "Dist_to_asymptotes: %.9f\n", rhc->rhc_c * base2local); 
}

C_DECL int
rt_edit_rhc_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    struct rt_rhc_internal *rhc = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(rhc);
    struct rt_rhc_internal candidate = *rhc;
    const struct edit_param_field fields[] = {
	{"Vertex", candidate.rhc_V, ELEMENTS_PER_VECT, local2base},
	{"Height", candidate.rhc_H, ELEMENTS_PER_VECT, local2base},
	{"Breadth", candidate.rhc_B, ELEMENTS_PER_VECT, local2base},
	{"Half-width", &candidate.rhc_r, 1, local2base},
	{"Dist_to_asymptotes", &candidate.rhc_c, 1, local2base}
    };
    if (edit_param_read_fields(fc, fields, sizeof(fields) / sizeof(fields[0])) != BRLCAD_OK)
	return BRLCAD_ERROR;
    *rhc = candidate;
    return BRLCAD_OK;
}

static int
rt_edit_rhc_pscale(struct rt_edit *s)
{
    struct rt_rhc_internal *rhc = (struct rt_rhc_internal *)s->es_int.idb_ptr;
    RT_RHC_CK_MAGIC(rhc);

    vect_t *axis = NULL;
    fastf_t *radius = NULL;
    switch (s->edit_flag) {
	case ECMD_RHC_B:
	    axis = &rhc->rhc_B;
	    break;
	case ECMD_RHC_H:
	    axis = &rhc->rhc_H;
	    break;
	case ECMD_RHC_R:
	    radius = &rhc->rhc_r;
	    break;
	case ECMD_RHC_C:
	    radius = &rhc->rhc_c;
	    break;
	default:
	    return BRLCAD_ERROR;
    }
    return edit_scale_length(s, axis, radius);
}

C_DECL int
rt_edit_rhc_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case ECMD_RHC_B:
	case ECMD_RHC_H:
	case ECMD_RHC_R:
	case ECMD_RHC_C:
	    return rt_edit_rhc_pscale(s);
	default:
	    return edit_generic(s);
    }
}

C_DECL int
rt_edit_rhc_edit_xy(
        struct rt_edit *s,
        const vect_t mousevec
        )
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */

    switch (s->edit_flag) {
        case RT_PARAMS_EDIT_SCALE:
	case ECMD_RHC_B:
	case ECMD_RHC_H:
	case ECMD_RHC_R:
	case ECMD_RHC_C:
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


int
rt_edit_rhc_repair(struct bu_vls *log_str, struct rt_db_internal *ip, const struct bn_tol *tol, int argc, const char **argv)
{
    struct rt_rhc_internal *rhc;
    fastf_t mag_b, mag_h;
    int repaired = 0;
    int options_json = 0;
    int print_help = 0;

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help", "", NULL, &print_help, "Print help");
    BU_OPT(d[1], "", "options-json", "", NULL, &options_json, "Return JSON of supported options");
    BU_OPT_NULL(d[2]);

    if (edit_repair_parse_options(log_str, argc, argv, d) != BRLCAD_OK)
        return -1;

    if (options_json) {
        if (log_str) {
            bu_vls_printf(log_str, "{\"options\":[]}");
        }
        return 1;
    }

    if (print_help) {
        if (log_str) {
            char *option_help = bu_opt_describe(d, NULL);
            bu_vls_printf(log_str, "{\"status\":\"help\",\"message\":\"Options:\\n%s\"}", option_help ? option_help : "");
            if (option_help) bu_free(option_help, "help str");
        }
        return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    rhc = (struct rt_rhc_internal *)ip->idb_ptr;
    RT_RHC_CK_MAGIC(rhc);

    if (!tol) {
        static const struct bn_tol default_tol = BN_TOL_INIT_TOL;
        tol = &default_tol;
    }

    mag_b = MAGNITUDE(rhc->rhc_B);
    mag_h = MAGNITUDE(rhc->rhc_H);

    if (mag_b > SQRT_SMALL_FASTF && mag_h > SQRT_SMALL_FASTF) {
        fastf_t f = VDOT(rhc->rhc_B, rhc->rhc_H) / (mag_b * mag_h);
        if (!NEAR_ZERO(f, tol->perp)) {
            vect_t proj;
            VSCALE(proj, rhc->rhc_H, VDOT(rhc->rhc_B, rhc->rhc_H) / MAGSQ(rhc->rhc_H));
            VSUB2(rhc->rhc_B, rhc->rhc_B, proj);
            /* Restore length */
            VSCALE(rhc->rhc_B, rhc->rhc_B, mag_b / MAGNITUDE(rhc->rhc_B));
            repaired++;
        }
    }

    if (repaired > 0 && log_str) {
        bu_vls_printf(log_str, "{\"status\":\"success\",\"message\":\"Successfully repaired RHC\"}");
    }

    return repaired > 0 ? 0 : -1;
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
