/*                        E D R E C . C
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
/** @file primitives/rec/edrec.c
 *
 * Editing support for the Right Elliptic Cylinder (REC) primitive.
 *
 * REC is an alias for TGC with:
 *   - A perp H, B perp H, A perp B, C = A, D = B
 *
 * Five REC-specific operations enforce these constraints:
 *   ECMD_REC_SET_V   – move the base centre point
 *   ECMD_REC_SET_H   – set the height vector (re-orthogonalise A, B)
 *   ECMD_REC_SCALE_R1 – set radius 1 (|A| = |C|)
 *   ECMD_REC_SCALE_R2 – set radius 2 (|B| = |D|)
 *   ECMD_REC_SCALE_R  – set both radii equal
 *
 * For all other modes the call falls through to rt_edit_tgc_edit().
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

#define ECMD_REC_SET_V    7001  /* Set base-centre vertex              */
#define ECMD_REC_SET_H    7002  /* Set height vector (re-orthogonalise)*/
#define ECMD_REC_SCALE_R1 7003  /* Set radius 1: |A| = |C|             */
#define ECMD_REC_SCALE_R2 7004  /* Set radius 2: |B| = |D|             */
#define ECMD_REC_SCALE_R  7005  /* Set both radii equal (RCC mode)     */

/* ------------------------------------------------------------------ */
/* ft_edit_desc descriptor for the Right Elliptic Cylinder primitive  */
/* ------------------------------------------------------------------ */

static const struct rt_edit_param_desc rec_v_params[] = {
    {
	"v",                        /* name */
	"Base Center (X Y Z)",      /* label */
	RT_EDIT_PARAM_POINT,        /* type */
	0,                          /* index */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_min */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max */
	"length",                   /* units */
	0, NULL, NULL,              /* enum */
	NULL                        /* prim_field */
    }
};

static const struct rt_edit_param_desc rec_h_params[] = {
    {
	"h",                        /* name */
	"Height Vector (X Y Z)",    /* label */
	RT_EDIT_PARAM_VECTOR,       /* type */
	0,                          /* index */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_min */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max */
	"length",                   /* units */
	0, NULL, NULL,              /* enum */
	NULL                        /* prim_field */
    }
};

static const struct rt_edit_param_desc rec_r1_params[] = {
    {
	"r1",                       /* name */
	"Radius 1",                 /* label */
	RT_EDIT_PARAM_SCALAR,       /* type */
	0,                          /* index */
	1e-10,                      /* range_min */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max */
	"length",                   /* units */
	0, NULL, NULL,              /* enum */
	NULL                        /* prim_field */
    }
};

static const struct rt_edit_param_desc rec_r2_params[] = {
    {
	"r2",                       /* name */
	"Radius 2",                 /* label */
	RT_EDIT_PARAM_SCALAR,       /* type */
	0,                          /* index */
	1e-10,                      /* range_min */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max */
	"length",                   /* units */
	0, NULL, NULL,              /* enum */
	NULL                        /* prim_field */
    }
};

static const struct rt_edit_param_desc rec_r_params[] = {
    {
	"r",                        /* name */
	"Radius (R1=R2)",           /* label */
	RT_EDIT_PARAM_SCALAR,       /* type */
	0,                          /* index */
	1e-10,                      /* range_min */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max */
	"length",                   /* units */
	0, NULL, NULL,              /* enum */
	NULL                        /* prim_field */
    }
};

static const struct rt_edit_cmd_desc rec_cmds[] = {
    {
	ECMD_REC_SET_V,             /* cmd_id */
	"Set V",                    /* label */
	"geometry",                 /* category */
	1,                          /* nparam */
	rec_v_params,               /* params */
	0,                          /* interactive */
	10                          /* display_order */
    },
    {
	ECMD_REC_SET_H,             /* cmd_id */
	"Set H",                    /* label */
	"geometry",                 /* category */
	1,                          /* nparam */
	rec_h_params,               /* params */
	0,                          /* interactive */
	20                          /* display_order */
    },
    {
	ECMD_REC_SCALE_R1,          /* cmd_id */
	"Set Radius 1",             /* label */
	"geometry",                 /* category */
	1,                          /* nparam */
	rec_r1_params,              /* params */
	0,                          /* interactive */
	30                          /* display_order */
    },
    {
	ECMD_REC_SCALE_R2,          /* cmd_id */
	"Set Radius 2",             /* label */
	"geometry",                 /* category */
	1,                          /* nparam */
	rec_r2_params,              /* params */
	0,                          /* interactive */
	40                          /* display_order */
    },
    {
	ECMD_REC_SCALE_R,           /* cmd_id */
	"Set Radius",               /* label */
	"geometry",                 /* category */
	1,                          /* nparam */
	rec_r_params,               /* params */
	0,                          /* interactive */
	50                          /* display_order */
    }
};

static const struct rt_edit_prim_desc rec_prim_desc = {
    "rec",                          /* prim_type */
    "Right Elliptic Cylinder",      /* prim_label */
    5,                              /* ncmd */
    rec_cmds                        /* cmds */
};

const struct rt_edit_prim_desc *
rt_edit_rec_edit_desc(void)
{
    return &rec_prim_desc;
}

void
rt_edit_rec_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_REC_SET_V:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_REC_SET_H:
	case ECMD_REC_SCALE_R1:
	case ECMD_REC_SCALE_R2:
	case ECMD_REC_SCALE_R:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	default:
	    break;
    }
}

/* Forward declare TGC functions used by REC as fallback */
extern int rt_edit_tgc_edit(struct rt_edit *s);
extern int rt_edit_tgc_edit_xy(struct rt_edit *s, const vect_t mousevec);

int
rt_edit_rec_edit(struct rt_edit *s)
{
    struct rt_tgc_internal *tgc;
    fastf_t r, mag_a, mag_b;
    vect_t h_unit, a_unit, b_unit, a_perp;
    fastf_t dot;

    switch (s->edit_flag) {
	case ECMD_REC_SET_V:
	    tgc = (struct rt_tgc_internal *)s->es_int.idb_ptr;
	    RT_TGC_CK_MAGIC(tgc);
	    if (s->e_inpara == 3) {
		VSCALE(tgc->v, s->e_para, s->local2base);
	    }
	    return BRLCAD_OK;

	case ECMD_REC_SET_H:
	    tgc = (struct rt_tgc_internal *)s->es_int.idb_ptr;
	    RT_TGC_CK_MAGIC(tgc);
	    if (s->e_inpara == 3) {
		vect_t new_h;
		VSCALE(new_h, s->e_para, s->local2base);
		if (MAGNITUDE(new_h) < SMALL_FASTF) {
		    bu_vls_printf(s->log_str,
			"ERROR: H vector must have non-zero length\n");
		    return BRLCAD_ERROR;
		}

		mag_a = MAGNITUDE(tgc->a);
		mag_b = MAGNITUDE(tgc->b);

		VMOVE(tgc->h, new_h);
		VMOVE(h_unit, new_h);
		VUNITIZE(h_unit);

		/* Re-orthogonalise A relative to new H (Gram-Schmidt) */
		dot = VDOT(tgc->a, h_unit);
		VJOIN1(a_perp, tgc->a, -dot, h_unit);
		if (MAGNITUDE(a_perp) < SMALL_FASTF)
		    bn_vec_perp(a_perp, h_unit);
		VMOVE(a_unit, a_perp);
		VUNITIZE(a_unit);

		/* B_unit = cross(H_unit, A_unit) */
		VCROSS(b_unit, h_unit, a_unit);
		VUNITIZE(b_unit);

		VSCALE(tgc->a, a_unit, mag_a);
		VSCALE(tgc->b, b_unit, mag_b);
		VMOVE(tgc->c, tgc->a);  /* REC: C = A */
		VMOVE(tgc->d, tgc->b);  /* REC: D = B */
	    }
	    return BRLCAD_OK;

	case ECMD_REC_SCALE_R1:
	    tgc = (struct rt_tgc_internal *)s->es_int.idb_ptr;
	    RT_TGC_CK_MAGIC(tgc);
	    if (s->e_inpara == 1) {
		r = s->e_para[0] * s->local2base;
		if (r < SMALL_FASTF) {
		    bu_vls_printf(s->log_str,
			"ERROR: radius must be positive\n");
		    return BRLCAD_ERROR;
		}
		mag_a = MAGNITUDE(tgc->a);
		if (mag_a < SMALL_FASTF) mag_a = 1.0;
		VSCALE(tgc->a, tgc->a, r / mag_a);
		VMOVE(tgc->c, tgc->a);
	    }
	    return BRLCAD_OK;

	case ECMD_REC_SCALE_R2:
	    tgc = (struct rt_tgc_internal *)s->es_int.idb_ptr;
	    RT_TGC_CK_MAGIC(tgc);
	    if (s->e_inpara == 1) {
		r = s->e_para[0] * s->local2base;
		if (r < SMALL_FASTF) {
		    bu_vls_printf(s->log_str,
			"ERROR: radius must be positive\n");
		    return BRLCAD_ERROR;
		}
		mag_b = MAGNITUDE(tgc->b);
		if (mag_b < SMALL_FASTF) mag_b = 1.0;
		VSCALE(tgc->b, tgc->b, r / mag_b);
		VMOVE(tgc->d, tgc->b);
	    }
	    return BRLCAD_OK;

	case ECMD_REC_SCALE_R:
	    tgc = (struct rt_tgc_internal *)s->es_int.idb_ptr;
	    RT_TGC_CK_MAGIC(tgc);
	    if (s->e_inpara == 1) {
		r = s->e_para[0] * s->local2base;
		if (r < SMALL_FASTF) {
		    bu_vls_printf(s->log_str,
			"ERROR: radius must be positive\n");
		    return BRLCAD_ERROR;
		}
		mag_a = MAGNITUDE(tgc->a);
		mag_b = MAGNITUDE(tgc->b);
		if (mag_a < SMALL_FASTF) mag_a = 1.0;
		if (mag_b < SMALL_FASTF) mag_b = 1.0;
		VSCALE(tgc->a, tgc->a, r / mag_a);
		VSCALE(tgc->b, tgc->b, r / mag_b);
		VMOVE(tgc->c, tgc->a);
		VMOVE(tgc->d, tgc->b);
	    }
	    return BRLCAD_OK;

	default:
	    break;
    }
    return rt_edit_tgc_edit(s);
}

int
rt_edit_rec_edit_xy(struct rt_edit *s, const vect_t mousevec)
{
    return rt_edit_tgc_edit_xy(s, mousevec);
}

int
rt_edit_rec_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    struct rt_tgc_internal *tgc;
    if (!s || !vals)
	return BRLCAD_ERROR;
    tgc = (struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    switch (cmd_id) {
	case ECMD_REC_SET_V:
	    vals[0] = tgc->v[X] * s->base2local;
	    vals[1] = tgc->v[Y] * s->base2local;
	    vals[2] = tgc->v[Z] * s->base2local;
	    return BRLCAD_OK;
	case ECMD_REC_SET_H:
	    vals[0] = tgc->h[X] * s->base2local;
	    vals[1] = tgc->h[Y] * s->base2local;
	    vals[2] = tgc->h[Z] * s->base2local;
	    return BRLCAD_OK;
	case ECMD_REC_SCALE_R1:
	    vals[0] = MAGNITUDE(tgc->a) * s->base2local;
	    return BRLCAD_OK;
	case ECMD_REC_SCALE_R2:
	    vals[0] = MAGNITUDE(tgc->b) * s->base2local;
	    return BRLCAD_OK;
	case ECMD_REC_SCALE_R:
	    vals[0] = (MAGNITUDE(tgc->a) + MAGNITUDE(tgc->b)) * 0.5 * s->base2local;
	    return BRLCAD_OK;
	default:
	    break;
    }
    return BRLCAD_ERROR;
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
