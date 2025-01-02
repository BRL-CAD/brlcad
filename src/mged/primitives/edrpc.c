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

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
mged_rpc_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(rpc->rpc_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(rpc->rpc_H));
    bu_vls_printf(p, "Breadth: %.9f %.9f %.9f\n", V3BASE2LOCAL(rpc->rpc_B));
    bu_vls_printf(p, "Half-width: %.9f\n", rpc->rpc_r * base2local);
}


#define read_params_line_incr \
    lc = (ln) ? (ln + lcj) : NULL; \
    if (!lc) { \
	bu_free(wc, "wc"); \
	return BRLCAD_ERROR; \
    } \
    ln = strchr(lc, tc); \
    if (ln) *ln = '\0'; \
    while (lc && strchr(lc, ':')) lc++

int
mged_rpc_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    if (!fc)
	return BRLCAD_ERROR;

    // We're getting the file contents as a string, so we need to split it up
    // to process lines. See https://stackoverflow.com/a/17983619

    // Figure out if we need to deal with Windows line endings
    const char *crpos = strchr(fc, '\r');
    int crlf = (crpos && crpos[1] == '\n') ? 1 : 0;
    char tc = (crlf) ? '\r' : '\n';
    // If we're CRLF jump ahead another character.
    int lcj = (crlf) ? 2 : 1;

    char *ln = NULL;
    char *wc = bu_strdup(fc);
    char *lc = wc;

    // Set up initial line (Vertex)
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';

    // Trim off prefixes, if user left them in
    while (lc && strchr(lc, ':')) lc++;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(rpc->rpc_V, a, b, c);
    VSCALE(rpc->rpc_V, rpc->rpc_V, local2base);

    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(rpc->rpc_H, a, b, c);
    VSCALE(rpc->rpc_H, rpc->rpc_H, local2base);

    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(rpc->rpc_B, a, b, c);
    VSCALE(rpc->rpc_B, rpc->rpc_B, local2base);

    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    rpc->rpc_r = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
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
