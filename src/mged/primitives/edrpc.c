/*                         E D R P C . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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
#include "./mged_functab.h"

#define MENU_RPC_B		17043
#define MENU_RPC_H		17044
#define MENU_RPC_R		17045

static void
rpc_ed(struct mged_solid_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    s->edit_menu = arg;
    mged_set_edflag(s, PSCALE);

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    mged_sedit_clbk_get(&f, &d, s, ECMD_EAXES_POS, 0, GED_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
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

    // Set up Height line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(rpc->rpc_H, a, b, c);
    VSCALE(rpc->rpc_H, rpc->rpc_H, local2base);

    // Set up Breadth line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(rpc->rpc_B, a, b, c);
    VSCALE(rpc->rpc_B, rpc->rpc_B, local2base);

    // Set up Half-width line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    rpc->rpc_r = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale vector B */
void
menu_rpc_b(struct mged_solid_edit *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->es_int.idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(rpc->rpc_B);
    }
    VSCALE(rpc->rpc_B, rpc->rpc_B, s->es_scale);
}

/* scale vector H */
void
menu_rpc_h(struct mged_solid_edit *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->es_int.idb_ptr;

    RT_RPC_CK_MAGIC(rpc);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(rpc->rpc_H);
    }
    VSCALE(rpc->rpc_H, rpc->rpc_H, s->es_scale);
}

/* scale rectangular half-width of RPC */
void
menu_rpc_r(struct mged_solid_edit *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->es_int.idb_ptr;

    RT_RPC_CK_MAGIC(rpc);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / rpc->rpc_r;
    }
    rpc->rpc_r *= s->es_scale;
}

static int
mged_rpc_pscale(struct mged_solid_edit *s, int mode)
{
    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return TCL_ERROR;
    }

    if (s->e_para[0] <= 0.0) {
	bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	s->e_inpara = 0;
	return TCL_ERROR;
    }

    /* must convert to base units */
    s->e_para[0] *= s->local2base;
    s->e_para[1] *= s->local2base;
    s->e_para[2] *= s->local2base;

    switch (mode) {
	case MENU_RPC_B:
	    menu_rpc_b(s);
	    break;
	case MENU_RPC_H:
	    menu_rpc_h(s);
	    break;
	case MENU_RPC_R:
	    menu_rpc_r(s);
	    break;
    };

    return 0;
}

int
mged_rpc_edit(struct mged_solid_edit *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    return mged_generic_sscale(s, &s->es_int);
	case STRANS:
	    /* translate solid */
	    mged_generic_strans(s, &s->es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    mged_generic_srot(s, &s->es_int);
	    break;
	case PSCALE:
	    return mged_rpc_pscale(s, s->edit_menu);
    }
    return 0;
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
