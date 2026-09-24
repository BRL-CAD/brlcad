/*                         E D E P A . C
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
/** @file primitives/edepa.c
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

#define ECMD_EPA_H		19050
#define ECMD_EPA_R1		19051
#define ECMD_EPA_R2		19052

C_DECL void
rt_edit_epa_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_EPA_H:
	case ECMD_EPA_R1:
	case ECMD_EPA_R2:
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
epa_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_epa_set_edit_mode(s, arg);
}

struct rt_edit_menu_item epa_menu[] = {
    { "EPA MENU", NULL, 0 },
    { "Set H", epa_ed, ECMD_EPA_H },
    { "Set A", epa_ed, ECMD_EPA_R1 },
    { "Set B", epa_ed, ECMD_EPA_R2 },
    { "", NULL, 0 }
};

C_DECL struct rt_edit_menu_item *
rt_edit_epa_menu_item(const struct bn_tol *UNUSED(tol))
{
    return epa_menu;
}

/* ft_edit_desc descriptor for the Elliptical Paraboloid primitive   */
/* ------------------------------------------------------------------ */

static const struct rt_edit_param_desc epa_h_params[] = {
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

static const struct rt_edit_param_desc epa_r1_params[] = {
    {
	"r1",                 /* name         */
	"Semi-Axis A",        /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_param_desc epa_r2_params[] = {
    {
	"r2",                 /* name         */
	"Semi-Axis B",        /* label        */
	RT_EDIT_PARAM_SCALAR, /* type         */
	0,                    /* index        */
	1e-10,                /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"length",             /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc epa_cmds[] = {
    {
	ECMD_EPA_H,           /* cmd_id       */
	"Set H",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	epa_h_params,         /* params       */
	1,                    /* interactive  */
	10                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_EPA_R1,          /* cmd_id       */
	"Set A",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	epa_r1_params,        /* params       */
	1,                    /* interactive  */
	20                    /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_EPA_R2,          /* cmd_id       */
	"Set B",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	epa_r2_params,        /* params       */
	1,                    /* interactive  */
	30                    /* display_order */,
	NULL                  /* req_types */
    }
};

static const struct rt_edit_prim_desc epa_prim_desc = {
    "epa",                /* prim_type    */
    "Elliptical Paraboloid", /* prim_label */
    3,                    /* ncmd         */
    epa_cmds              /* cmds         */,
    0,                    /* nopt         */
    NULL                  /* opts         */
};

C_DECL const struct rt_edit_prim_desc *
rt_edit_epa_edit_desc(void)
{
    return &epa_prim_desc;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

C_DECL void
rt_edit_epa_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_epa_internal *epa = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(epa);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(epa->epa_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(epa->epa_H));
    bu_vls_printf(p, "Semi-major axis: %.9f %.9f %.9f\n", V3ARGS(epa->epa_Au));
    bu_vls_printf(p, "Semi-major length: %.9f\n", epa->epa_r1 * base2local);
    bu_vls_printf(p, "Semi-minor length: %.9f\n", epa->epa_r2 * base2local);
}

C_DECL int
rt_edit_epa_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    struct rt_epa_internal *epa = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(epa);
    struct rt_epa_internal candidate = *epa;
    const struct edit_param_field fields[] = {
	{"Vertex", candidate.epa_V, ELEMENTS_PER_VECT, local2base},
	{"Height", candidate.epa_H, ELEMENTS_PER_VECT, local2base},
	{"Semi-major axis", candidate.epa_Au, ELEMENTS_PER_VECT, 1.0},
	{"Semi-major length", &candidate.epa_r1, 1, local2base},
	{"Semi-minor length", &candidate.epa_r2, 1, local2base}
    };
    if (edit_param_read_fields(fc, fields, sizeof(fields) / sizeof(fields[0])) != BRLCAD_OK ||
	ZERO(MAGNITUDE(candidate.epa_Au)))
	return BRLCAD_ERROR;
    VUNITIZE(candidate.epa_Au);
    *epa = candidate;
    return BRLCAD_OK;
}

/* scale height vector H */
void
ecmd_epa_h(struct rt_edit *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    VSCALE(epa->epa_H, epa->epa_H, s->es_scale);
}

/* scale semimajor axis of EPA */
static int
ecmd_epa_r1(struct rt_edit *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    if (epa->epa_r1 * s->es_scale >= epa->epa_r2) {
	epa->epa_r1 *= s->es_scale;
	return BRLCAD_OK;
    }
    bu_vls_printf(s->log_str, "Semi-major axis cannot be shorter than semi-minor axis\n");
    return BRLCAD_ERROR;
}

/* scale semiminor axis of EPA */
static int
ecmd_epa_r2(struct rt_edit *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    if (epa->epa_r2 * s->es_scale <= epa->epa_r1) {
	epa->epa_r2 *= s->es_scale;
	return BRLCAD_OK;
    }
    bu_vls_printf(s->log_str, "Semi-minor axis cannot be longer than semi-major axis\n");
    return BRLCAD_ERROR;
}

static int
rt_edit_epa_pscale(struct rt_edit *s)
{
    struct rt_epa_internal *epa = (struct rt_epa_internal *)s->es_int.idb_ptr;
    RT_EPA_CK_MAGIC(epa);
    if (!s->e_inpara && ZERO(s->es_scale))
	return BRLCAD_OK;

    fastf_t current;
    switch (s->edit_flag) {
	case ECMD_EPA_H:
	    current = MAGNITUDE(epa->epa_H);
	    break;
	case ECMD_EPA_R1:
	    current = epa->epa_r1;
	    break;
	case ECMD_EPA_R2:
	    current = epa->epa_r2;
	    break;
	default:
	    return BRLCAD_ERROR;
    }
    if (edit_prepare_length_scale(s, current) != BRLCAD_OK)
	return BRLCAD_ERROR;

    switch (s->edit_flag) {
	case ECMD_EPA_H:
	    ecmd_epa_h(s);
	    break;
	case ECMD_EPA_R1:
	    return ecmd_epa_r1(s);
	case ECMD_EPA_R2:
	    return ecmd_epa_r2(s);
    };

    return 0;
}

C_DECL int
rt_edit_epa_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case ECMD_EPA_H:
	case ECMD_EPA_R1:
	case ECMD_EPA_R2:
	    return rt_edit_epa_pscale(s);
	default:
	    return edit_generic(s);
    }
}

C_DECL int
rt_edit_epa_edit_xy(
        struct rt_edit *s,
        const vect_t mousevec
        )
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */

    switch (s->edit_flag) {
        case RT_PARAMS_EDIT_SCALE:
	case ECMD_EPA_H:
	case ECMD_EPA_R1:
	case ECMD_EPA_R2:
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
rt_edit_epa_repair(struct bu_vls *log_str, struct rt_db_internal *ip, const struct bn_tol *tol, int argc, const char **argv)
{
    struct rt_epa_internal *epa;
    fastf_t mag_h;
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
    epa = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(epa);

    if (!tol) {
        static const struct bn_tol default_tol = BN_TOL_INIT_TOL;
        tol = &default_tol;
    }

    mag_h = MAGNITUDE(epa->epa_H);

    if (!NEAR_EQUAL(MAGSQ(epa->epa_Au), 1.0, tol->dist)) {
        fastf_t mag_au = MAGNITUDE(epa->epa_Au);
        if (mag_au > SQRT_SMALL_FASTF) {
            VSCALE(epa->epa_Au, epa->epa_Au, 1.0 / mag_au);
            repaired++;
        }
    }

    if (mag_h > SQRT_SMALL_FASTF) {
        fastf_t f = VDOT(epa->epa_Au, epa->epa_H) / mag_h;
        if (!NEAR_ZERO(f, tol->perp)) {
            vect_t proj;
            VSCALE(proj, epa->epa_H, VDOT(epa->epa_Au, epa->epa_H) / MAGSQ(epa->epa_H));
            VSUB2(epa->epa_Au, epa->epa_Au, proj);
            VUNITIZE(epa->epa_Au);
            repaired++;
        }
    }

    if (repaired > 0 && log_str) {
        bu_vls_printf(log_str, "{\"status\":\"success\",\"message\":\"Successfully repaired EPA\"}");
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
