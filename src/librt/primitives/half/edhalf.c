/*                         E D H A L F . C
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
/** @file primitives/edhalf.c
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

#define V4BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local, (_pt)[W]*base2local

void
rt_edit_hlf_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_half_internal *half = (struct rt_half_internal *)ip->idb_ptr;
    RT_HALF_CK_MAGIC(half);

    bu_vls_printf(p, "Plane: %.9f %.9f %.9f %.9f\n", V4BASE2LOCAL(half->eqn));
}

int
rt_edit_hlf_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;
    struct rt_half_internal *haf = (struct rt_half_internal *)ip->idb_ptr;
    RT_HALF_CK_MAGIC(haf);

    if (!fc)
	return BRLCAD_ERROR;

    const char *lc = fc;
    while (lc && strchr(lc, ':')) lc++;

    sscanf(lc, "%lf %lf %lf %lf", &a, &b, &c, &d);
    VSET(haf->eqn, a, b, c);
    haf->eqn[W] = d * local2base;

    // Cleanup
    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* ft_edit_desc descriptor for the Halfspace primitive                  */
/* ------------------------------------------------------------------ */

#define ECMD_HALF_SET_D  6001  /* Set plane distance (D coefficient)  */

static const struct rt_edit_param_desc half_d_params[] = {
    {
	"d",                    /* name */
	"Plane Distance",       /* label */
	RT_EDIT_PARAM_SCALAR,   /* type */
	0,                      /* index */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max */
	"length",               /* units */
	0, NULL, NULL,          /* enum */
	NULL                    /* prim_field */
    }
};

static const struct rt_edit_cmd_desc half_cmds[] = {
    {
	ECMD_HALF_SET_D,        /* cmd_id */
	"Set D",                /* label */
	"geometry",             /* category */
	1,                      /* nparam */
	half_d_params,          /* params */
	0,                      /* interactive */
	10                      /* display_order */
    }
};

static const struct rt_edit_prim_desc half_prim_desc = {
    "half",                     /* prim_type */
    "Halfspace",                /* prim_label */
    1,                          /* ncmd */
    half_cmds                   /* cmds */
};

const struct rt_edit_prim_desc *
rt_edit_hlf_edit_desc(void)
{
    return &half_prim_desc;
}

void
rt_edit_hlf_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);
}

int
rt_edit_hlf_edit(struct rt_edit *s)
{
    struct rt_half_internal *haf;
    switch (s->edit_flag) {
	case ECMD_HALF_SET_D:
	    haf = (struct rt_half_internal *)s->es_int.idb_ptr;
	    RT_HALF_CK_MAGIC(haf);
	    if (s->e_inpara == 1)
		haf->eqn[W] = s->e_para[0] * s->local2base;
	    return BRLCAD_OK;
	default:
	    break;
    }
    return edit_generic(s);
}

int
rt_edit_hlf_edit_xy(struct rt_edit *s, const vect_t mousevec)
{
    return edit_generic_xy(s, mousevec);
}

int
rt_edit_hlf_get_params(struct rt_edit *s, int cmd_id, fastf_t *vals)
{
    struct rt_half_internal *haf;
    if (!s || !vals)
	return BRLCAD_ERROR;
    haf = (struct rt_half_internal *)s->es_int.idb_ptr;
    RT_HALF_CK_MAGIC(haf);
    if (cmd_id == ECMD_HALF_SET_D) {
	vals[0] = haf->eqn[W] * s->base2local;
	return BRLCAD_OK;
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
