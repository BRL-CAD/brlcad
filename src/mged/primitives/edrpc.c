/*                         E D R P C . C
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
/** @file mged/primitives/edrpc.c
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
#include "./edrpc.h"

static void
rpc_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;

    set_e_axes_pos(s, 1);
}
struct menu_item rpc_menu[] = {
    { "RPC MENU", NULL, 0 },
    { "Set B", rpc_ed, MENU_RPC_B },
    { "Set H", rpc_ed, MENU_RPC_H },
    { "Set r", rpc_ed, MENU_RPC_R },
    { "", NULL, 0 }
};

struct menu_item *
mged_rpc_menu_item(const struct bn_tol *UNUSED(tol))
{
    return rpc_menu;
}

/* scale vector B */
void
menu_rpc_b(struct mged_state *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->edit_state.es_int.idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(rpc->rpc_B);
    }
    VSCALE(rpc->rpc_B, rpc->rpc_B, s->edit_state.es_scale);
}

/* scale vector H */
void
menu_rpc_h(struct mged_state *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->edit_state.es_int.idb_ptr;

    RT_RPC_CK_MAGIC(rpc);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / MAGNITUDE(rpc->rpc_H);
    }
    VSCALE(rpc->rpc_H, rpc->rpc_H, s->edit_state.es_scale);
}

/* scale rectangular half-width of RPC */
void
menu_rpc_r(struct mged_state *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->edit_state.es_int.idb_ptr;

    RT_RPC_CK_MAGIC(rpc);
    if (inpara) {
	/* take es_mat[15] (path scaling) into account */
	es_para[0] *= es_mat[15];
	s->edit_state.es_scale = es_para[0] / rpc->rpc_r;
    }
    rpc->rpc_r *= s->edit_state.es_scale;
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
