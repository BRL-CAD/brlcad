/*                         E D E P A . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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

void
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

struct rt_edit_menu_item *
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
	10                    /* display_order */
    },
    {
	ECMD_EPA_R1,          /* cmd_id       */
	"Set A",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	epa_r1_params,        /* params       */
	1,                    /* interactive  */
	20                    /* display_order */
    },
    {
	ECMD_EPA_R2,          /* cmd_id       */
	"Set B",              /* label        */
	"geometry",           /* category     */
	1,                    /* nparam       */
	epa_r2_params,        /* params       */
	1,                    /* interactive  */
	30                    /* display_order */
    }
};

static const struct rt_edit_prim_desc epa_prim_desc = {
    "epa",                /* prim_type    */
    "Elliptical Paraboloid", /* prim_label */
    3,                    /* ncmd         */
    epa_cmds              /* cmds         */
};

const struct rt_edit_prim_desc *
rt_edit_epa_edit_desc(void)
{
    return &epa_prim_desc;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
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

#define read_params_line_incr \
    lc = (ln) ? (ln + lcj) : NULL; \
    if (!lc) { \
	bu_free(wc, "wc"); \
	return BRLCAD_ERROR; \
    } \
    ln = strchr(lc, tc); \
    if (ln) *ln = '\0'; \
    while (lc && strchr(lc, ':')) lc++

int
rt_edit_epa_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_epa_internal *epa = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(epa);

    if (!fc)
	return BRLCAD_ERROR;

    // We're getting the file contents as a string, so we need to split it up
    // to process lines. See https://stackoverflow.com/a/17983619

    // Figure out if we need to deal with Windows line endings
    const char *crpos = strchr(fc, '\r');
    int crlf = (crpos && crpos[1] == '\n') ? 1 : 0;
    char tc = (crlf) ? '\r' : '\n';
    // If we're CRLF jump ahead another character.
    int lcj = (crlf) ? 2 : 1;

    char *ln = NULL;
    char *wc = bu_strdup(fc);
    char *lc = wc;

    // Set up initial line (Vertex)
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';

    // Trim off prefixes, if user left them in
    while (lc && strchr(lc, ':')) lc++;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(epa->epa_V, a, b, c);
    VSCALE(epa->epa_V, epa->epa_V, local2base);

    // Set up Height line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(epa->epa_H, a, b, c);
    VSCALE(epa->epa_H, epa->epa_H, local2base);

    // Set up Semi-major axis line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(epa->epa_Au, a, b, c);
    VUNITIZE(epa->epa_Au);

    // Set up Semi-major length line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    epa->epa_r1 = a * local2base;

    // Set up Semi-minor length line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    epa->epa_r2 = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale height vector H */
void
ecmd_epa_h(struct rt_edit *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(epa->epa_H);
    }
    VSCALE(epa->epa_H, epa->epa_H, s->es_scale);
}

/* scale semimajor axis of EPA */
void
ecmd_epa_r1(struct rt_edit *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / epa->epa_r1;
    }
    if (epa->epa_r1 * s->es_scale >= epa->epa_r2)
	epa->epa_r1 *= s->es_scale;
    else
	bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
}

/* scale semiminor axis of EPA */
void
ecmd_epa_r2(struct rt_edit *s)
{
    struct rt_epa_internal *epa =
	(struct rt_epa_internal *)s->es_int.idb_ptr;

    RT_EPA_CK_MAGIC(epa);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / epa->epa_r2;
    }
    if (epa->epa_r2 * s->es_scale <= epa->epa_r1)
	epa->epa_r2 *= s->es_scale;
    else
	bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
}

static int
rt_edit_epa_pscale(struct rt_edit *s)
{
    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (s->e_inpara) {
	if (s->e_para[0] <= 0.0) {
	    bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;
    }

    switch (s->edit_flag) {
	case ECMD_EPA_H:
	    ecmd_epa_h(s);
	    break;
	case ECMD_EPA_R1:
	    ecmd_epa_r1(s);
	    break;
	case ECMD_EPA_R2:
	    ecmd_epa_r2(s);
	    break;
    };

    return 0;
}

int
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

int
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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
