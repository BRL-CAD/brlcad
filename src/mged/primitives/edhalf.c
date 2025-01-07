/*                         E D H A L F . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2024 United States Government as represented by
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
/** @file mged/primitives/edhalf.c
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

#include "../mged.h"
#include "../sedit.h"
#include "../mged_dm.h"
#include "./mged_functab.h"

#define V4BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local, (_pt)[W]*base2local

void
mged_hlf_write_params(
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
mged_hlf_read_params(
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
