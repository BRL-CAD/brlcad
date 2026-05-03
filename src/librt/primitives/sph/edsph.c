/*                        E D S P H . C
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
/** @file primitives/sph/edsph.c
 *
 * Editing support for the Sphere (SPH) primitive.
 *
 * SPH is an alias for ELL with the constraint |A|=|B|=|C|.  The
 * descriptor exposed here wraps two sphere-specific operations that
 * honour that constraint:
 *   ECMD_SPH_SET_V   – move the centre point
 *   ECMD_SPH_SCALE_R – set the uniform radius (|A|=|B|=|C|)
 *
 * For all other edit modes the call falls through to rt_edit_ell_edit().
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

#define ECMD_SPH_SET_V    10001  /* Set sphere centre point            */
#define ECMD_SPH_SCALE_R  10002  /* Set uniform radius (|A|=|B|=|C|)  */

/* ------------------------------------------------------------------ */
/* ft_edit_desc descriptor for the Sphere primitive                    */
/* ------------------------------------------------------------------ */

static const struct rt_edit_param_desc sph_v_params[] = {
    {
	"v",                        /* name */
	"Center (X Y Z)",           /* label */
	RT_EDIT_PARAM_POINT,        /* type */
	0,                          /* index */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_min */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max */
	"length",                   /* units */
	0, NULL, NULL,              /* enum */
	NULL                        /* prim_field */
    }
};

static const struct rt_edit_param_desc sph_r_params[] = {
    {
	"r",                        /* name */
	"Radius",                   /* label */
	RT_EDIT_PARAM_SCALAR,       /* type */
	0,                          /* index */
	1e-10,                      /* range_min */
	RT_EDIT_PARAM_NO_LIMIT,     /* range_max */
	"length",                   /* units */
	0, NULL, NULL,              /* enum */
	NULL                        /* prim_field */
    }
};

static const struct rt_edit_cmd_desc sph_cmds[] = {
    {
	ECMD_SPH_SET_V,             /* cmd_id */
	"Set V",                    /* label */
	"geometry",                 /* category */
	1,                          /* nparam */
	sph_v_params,               /* params */
	0,                          /* interactive */
	10                          /* display_order */
    },
    {
	ECMD_SPH_SCALE_R,           /* cmd_id */
	"Set Radius",               /* label */
	"geometry",                 /* category */
	1,                          /* nparam */
	sph_r_params,               /* params */
	0,                          /* interactive */
	20                          /* display_order */
    }
};

static const struct rt_edit_prim_desc sph_prim_desc = {
    "sph",                          /* prim_type */
    "Sphere",                       /* prim_label */
    2,                              /* ncmd */
    sph_cmds                        /* cmds */
};

const struct rt_edit_prim_desc *
rt_edit_sph_edit_desc(void)
{
    return &sph_prim_desc;
}

void
rt_edit_sph_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_SPH_SET_V:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_SPH_SCALE_R:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	default:
	    break;
    }
}

/* Forward declare ELL functions used by SPH as fallback */
extern int rt_edit_ell_edit(struct rt_edit *s);
extern int rt_edit_ell_edit_xy(struct rt_edit *s, const vect_t mousevec);

int
rt_edit_sph_edit(struct rt_edit *s)
{
    struct rt_ell_internal *ell;
    fastf_t r, old_r;

    switch (s->edit_flag) {
	case ECMD_SPH_SET_V:
	    ell = (struct rt_ell_internal *)s->es_int.idb_ptr;
	    RT_ELL_CK_MAGIC(ell);
	    if (s->e_inpara == 3) {
		VSCALE(ell->v, s->e_para, s->local2base);
	    }
	    return BRLCAD_OK;

	case ECMD_SPH_SCALE_R:
	    ell = (struct rt_ell_internal *)s->es_int.idb_ptr;
	    RT_ELL_CK_MAGIC(ell);
	    if (s->e_inpara == 1) {
		r = s->e_para[0] * s->local2base;
		if (r < SMALL_FASTF) {
		    bu_vls_printf(s->log_str,
			"ERROR: radius must be positive\n");
		    return BRLCAD_ERROR;
		}
		old_r = MAGNITUDE(ell->a);
		if (old_r > SMALL_FASTF) {
		    VSCALE(ell->a, ell->a, r / old_r);
		    VSCALE(ell->b, ell->b, r / old_r);
		    VSCALE(ell->c, ell->c, r / old_r);
		} else {
		    VSET(ell->a, r, 0.0, 0.0);
		    VSET(ell->b, 0.0, r, 0.0);
		    VSET(ell->c, 0.0, 0.0, r);
		}
	    }
	    return BRLCAD_OK;

	default:
	    break;
    }
    return rt_edit_ell_edit(s);
}

int
rt_edit_sph_edit_xy(struct rt_edit *s, const vect_t mousevec)
{
    return rt_edit_ell_edit_xy(s, mousevec);
}

int
rt_edit_sph_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    struct rt_ell_internal *ell;
    if (!s || !vals)
	return BRLCAD_ERROR;
    ell = (struct rt_ell_internal *)s->es_int.idb_ptr;
    RT_ELL_CK_MAGIC(ell);

    switch (cmd_id) {
	case ECMD_SPH_SET_V:
	    vals[0] = ell->v[X] * s->base2local;
	    vals[1] = ell->v[Y] * s->base2local;
	    vals[2] = ell->v[Z] * s->base2local;
	    return BRLCAD_OK;
	case ECMD_SPH_SCALE_R:
	    vals[0] = MAGNITUDE(ell->a) * s->base2local;
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
