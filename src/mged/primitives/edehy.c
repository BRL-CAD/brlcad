/*                         E D E H Y . C
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
/** @file mged/primitives/edehy.c
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
#include "./edehy.h"

static void
ehy_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item ehy_menu[] = {
    { "EHY MENU", NULL, 0 },
    { "Set H", ehy_ed, MENU_EHY_H },
    { "Set A", ehy_ed, MENU_EHY_R1 },
    { "Set B", ehy_ed, MENU_EHY_R2 },
    { "Set c", ehy_ed, MENU_EHY_C },
    { "", NULL, 0 }
};

struct menu_item *
mged_ehy_menu_item(const struct bn_tol *UNUSED(tol))
{
    return ehy_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_ehy_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_ehy_internal *ehy = (struct rt_ehy_internal *)ip->idb_ptr;
    RT_EHY_CK_MAGIC(ehy);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(ehy->ehy_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(ehy->ehy_H));
    bu_vls_printf(p, "Semi-major axis: %.9f %.9f %.9f\n", V3ARGS(ehy->ehy_Au));
    bu_vls_printf(p, "Semi-major length: %.9f\n", ehy->ehy_r1 * base2local);
    bu_vls_printf(p, "Semi-minor length: %.9f\n", ehy->ehy_r2 * base2local);
    bu_vls_printf(p, "Dist to asymptotes: %.9f\n", ehy->ehy_c * base2local);
}

/* scale height vector H */
void
menu_ehy_h(struct mged_state *s)
{
    struct rt_ehy_internal *ehy =
	(struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

    RT_EHY_CK_MAGIC(ehy);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(ehy->ehy_H);
    }
    VSCALE(ehy->ehy_H, ehy->ehy_H, s->edit_state.es_scale);
}

/* scale semimajor axis of EHY */
void
menu_ehy_r1(struct mged_state *s)
{
    struct rt_ehy_internal *ehy =
	(struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

    RT_EHY_CK_MAGIC(ehy);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / ehy->ehy_r1;
    }
    if (ehy->ehy_r1 * s->edit_state.es_scale >= ehy->ehy_r2)
	ehy->ehy_r1 *= s->edit_state.es_scale;
    else
	bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
}

/* scale semiminor axis of EHY */
void
menu_ehy_r2(struct mged_state *s)
{
    struct rt_ehy_internal *ehy =
	(struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

    RT_EHY_CK_MAGIC(ehy);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / ehy->ehy_r2;
    }
    if (ehy->ehy_r2 * s->edit_state.es_scale <= ehy->ehy_r1)
	ehy->ehy_r2 *= s->edit_state.es_scale;
    else
	bu_log("pscale:  semi-minor axis cannot be longer than semi-major axis!");
}

/* scale distance between apex of EHY & asymptotic cone */
void
menu_ehy_c(struct mged_state *s)
{
    struct rt_ehy_internal *ehy =
	(struct rt_ehy_internal *)s->edit_state.es_int.idb_ptr;

    RT_EHY_CK_MAGIC(ehy);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / ehy->ehy_c;
    }
    ehy->ehy_c *= s->edit_state.es_scale;
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
