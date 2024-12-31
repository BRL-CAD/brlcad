/*                         E D T O R . C
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
/** @file mged/primitives/edtor.c
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
#include "./edtor.h"

static void
tor_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}


struct menu_item tor_menu[] = {
    { "TORUS MENU", NULL, 0 },
    { "Set Radius 1", tor_ed, MENU_TOR_R1 },
    { "Set Radius 2", tor_ed, MENU_TOR_R2 },
    { "", NULL, 0 }
};


struct menu_item *
mged_tor_menu_item(const struct bn_tol *UNUSED(tol))
{
    return tor_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_tor_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_tor_internal *tor = (struct rt_tor_internal *)ip->idb_ptr;
    RT_TOR_CK_MAGIC(tor);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(tor->v));
    bu_vls_printf(p, "Normal: %.9f %.9f %.9f\n", V3BASE2LOCAL(tor->h));
    bu_vls_printf(p, "radius_1: %.9f\n", tor->r_a*base2local);
    bu_vls_printf(p, "radius_2: %.9f\n", tor->r_h*base2local);
}

/* scale radius 1 of TOR */
void
menu_tor_r1(struct mged_state *s)
{
    struct rt_tor_internal *tor =
	(struct rt_tor_internal *)s->edit_state.es_int.idb_ptr;
    fastf_t newrad;
    RT_TOR_CK_MAGIC(tor);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	newrad = es_para[0];
    } else {
	newrad = tor->r_a * s->edit_state.es_scale;
    }
    if (newrad < SMALL) newrad = 4*SMALL;
    if (tor->r_h <= newrad)
	tor->r_a = newrad;
}

/* scale radius 2 of TOR */
void
menu_tor_r2(struct mged_state *s)
{
    struct rt_tor_internal *tor =
	(struct rt_tor_internal *)s->edit_state.es_int.idb_ptr;
    fastf_t newrad;
    RT_TOR_CK_MAGIC(tor);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	newrad = es_para[0];
    } else {
	newrad = tor->r_h * s->edit_state.es_scale;
    }
    if (newrad < SMALL) newrad = 4*SMALL;
    if (newrad <= tor->r_a)
	tor->r_h = newrad;
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
