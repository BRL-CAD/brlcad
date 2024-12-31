/*                         E D S U P E R E L L . C
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
/** @file mged/primitives/edsuperell.c
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
#include "./edsuperell.h"

static void
superell_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b)) {
    es_menu = arg;
    es_edflag = PSCALE;
    set_e_axes_pos(s, 1);
    return;
}
struct menu_item superell_menu[] = {
    { "SUPERELLIPSOID MENU", NULL, 0 },
    { "Set A", superell_ed, MENU_SUPERELL_SCALE_A },
    { "Set B", superell_ed, MENU_SUPERELL_SCALE_B },
    { "Set C", superell_ed, MENU_SUPERELL_SCALE_C },
    { "Set A,B,C", superell_ed, MENU_SUPERELL_SCALE_ABC },
    { "", NULL, 0 }
};

struct menu_item *
mged_superell_menu_item(const struct bn_tol *UNUSED(tol))
{
    return superell_menu;
}

/* scale vector A */
void
menu_superell_scale_a(struct mged_state *s)
{
    struct rt_superell_internal *superell =
	(struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	s->edit_state.es_scale = es_para[0] * es_mat[15] /
	    MAGNITUDE(superell->a);
    }
    VSCALE(superell->a, superell->a, s->edit_state.es_scale);
}

/* scale vector B */
void
menu_superell_scale_b(struct mged_state *s)
{
    struct rt_superell_internal *superell =
	(struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	s->edit_state.es_scale = es_para[0] * es_mat[15] /
	    MAGNITUDE(superell->b);
    }
    VSCALE(superell->b, superell->b, s->edit_state.es_scale);
}

/* scale vector C */
void
menu_superell_scale_c(struct mged_state *s)
{
    struct rt_superell_internal *superell =
	(struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	s->edit_state.es_scale = es_para[0] * es_mat[15] /
	    MAGNITUDE(superell->c);
    }
    VSCALE(superell->c, superell->c, s->edit_state.es_scale);
}

/* set A, B, and C length the same */
void
menu_superell_scale_abc(struct mged_state *s)
{
    fastf_t ma, mb;
    struct rt_superell_internal *superell =
	(struct rt_superell_internal *)s->edit_state.es_int.idb_ptr;
    RT_SUPERELL_CK_MAGIC(superell);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	s->edit_state.es_scale = es_para[0] * es_mat[15] /
	    MAGNITUDE(superell->a);
    }
    VSCALE(superell->a, superell->a, s->edit_state.es_scale);
    ma = MAGNITUDE(superell->a);
    mb = MAGNITUDE(superell->b);
    VSCALE(superell->b, superell->b, ma/mb);
    mb = MAGNITUDE(superell->c);
    VSCALE(superell->c, superell->c, ma/mb);
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
