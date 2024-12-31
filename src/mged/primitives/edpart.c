/*                         E D P A R T . C
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
/** @file mged/primitives/edpart.c
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
#include "./edpart.h"

static void
part_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item part_menu[] = {
    { "Particle MENU", NULL, 0 },
    { "Set H", part_ed, MENU_PART_H },
    { "Set v", part_ed, MENU_PART_v },
    { "Set h", part_ed, MENU_PART_h },
    { "", NULL, 0 }
};

struct menu_item *
mged_part_menu_item(const struct bn_tol *UNUSED(tol))
{
    return part_menu;
}

/* scale vector H */
void
menu_part_h(struct mged_state *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(part->part_H);
    }
    VSCALE(part->part_H, part->part_H, s->edit_state.es_scale);
}

/* scale v end radius */
void
menu_part_v(struct mged_state *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / part->part_vrad;
    }
    part->part_vrad *= s->edit_state.es_scale;
}

/* scale h end radius */
void
menu_part_h_end_r(struct mged_state *s)
{
    struct rt_part_internal *part =
	(struct rt_part_internal *)s->edit_state.es_int.idb_ptr;

    RT_PART_CK_MAGIC(part);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / part->part_hrad;
    }
    part->part_hrad *= s->edit_state.es_scale;
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
