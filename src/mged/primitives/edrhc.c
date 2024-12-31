/*                         E D R H C . C
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
/** @file mged/primitives/edrhc.c
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
#include "./edrhc.h"

static void
rhc_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item rhc_menu[] = {
    { "RHC MENU", NULL, 0 },
    { "Set B", rhc_ed, MENU_RHC_B },
    { "Set H", rhc_ed, MENU_RHC_H },
    { "Set r", rhc_ed, MENU_RHC_R },
    { "Set c", rhc_ed, MENU_RHC_C },
    { "", NULL, 0 }
};

struct menu_item *
mged_rhc_menu_item(const struct bn_tol *UNUSED(tol))
{
    return rhc_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_rhc_write_params(
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

/* scale vector B */
void
menu_rhc_b(struct mged_state *s)
{
    struct rt_rhc_internal *rhc =
	(struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;
    RT_RHC_CK_MAGIC(rhc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(rhc->rhc_B);
    }
    VSCALE(rhc->rhc_B, rhc->rhc_B, s->edit_state.es_scale);
}

/* scale vector H */
void
menu_rhc_h(struct mged_state *s)
{
    struct rt_rhc_internal *rhc =
	(struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;
    RT_RHC_CK_MAGIC(rhc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(rhc->rhc_H);
    }
    VSCALE(rhc->rhc_H, rhc->rhc_H, s->edit_state.es_scale);
}

/* scale rectangular half-width of RHC */
void
menu_rhc_r(struct mged_state *s)
{
    struct rt_rhc_internal *rhc =
	(struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;

    RT_RHC_CK_MAGIC(rhc);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / rhc->rhc_r;
    }
    rhc->rhc_r *= s->edit_state.es_scale;
}

/* scale rectangular half-width of RHC */
void
menu_rhc_c(struct mged_state *s)
{
    struct rt_rhc_internal *rhc =
	(struct rt_rhc_internal *)s->edit_state.es_int.idb_ptr;

    RT_RHC_CK_MAGIC(rhc);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / rhc->rhc_c;
    }
    rhc->rhc_c *= s->edit_state.es_scale;
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
