/*                      E D M E T A B A L L . C
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
/** @file mged/primitives/edmetaball.c
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
#include "./edmetaball.h"

struct wdb_metaball_pnt *es_metaball_pnt=(struct wdb_metaball_pnt *)NULL; /* Currently selected METABALL Point */

extern vect_t es_mparam;	/* mouse input param.  Only when es_mvalid set */
extern int es_mvalid;	/* es_mparam valid.  inpara must = 0 */

static void
metaball_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    struct wdb_metaball_pnt *next, *prev;

    if (s->dbip == DBI_NULL)
	return;

    switch (arg) {
	case MENU_METABALL_SET_THRESHOLD:
	    es_menu = arg;
	    es_edflag = PSCALE;
	    break;
	case MENU_METABALL_SET_METHOD:
	    es_menu = arg;
	    es_edflag = PSCALE;
	    break;
	case MENU_METABALL_PT_SET_GOO:
	    es_menu = arg;
	    es_edflag = PSCALE;
	    break;
	case MENU_METABALL_SELECT:
	    es_menu = arg;
	    es_edflag = ECMD_METABALL_PT_PICK;
	    break;
	case MENU_METABALL_NEXT_PT:
	    if (!es_metaball_pnt) {
		Tcl_AppendResult(s->interp, "No Metaball Point selected\n", (char *)NULL);
		return;
	    }
	    next = BU_LIST_NEXT(wdb_metaball_pnt, &es_metaball_pnt->l);
	    if (next->l.magic == BU_LIST_HEAD_MAGIC) {
		Tcl_AppendResult(s->interp, "Current point is the last\n", (char *)NULL);
		return;
	    }
	    es_metaball_pnt = next;
	    rt_metaball_pnt_print(es_metaball_pnt, s->dbip->dbi_base2local);
	    es_menu = arg;
	    es_edflag = IDLE;
	    sedit(s);
	    break;
	case MENU_METABALL_PREV_PT:
	    if (!es_metaball_pnt) {
		Tcl_AppendResult(s->interp, "No Metaball Point selected\n", (char *)NULL);
		return;
	    }
	    prev = BU_LIST_PREV(wdb_metaball_pnt, &es_metaball_pnt->l);
	    if (prev->l.magic == BU_LIST_HEAD_MAGIC) {
		Tcl_AppendResult(s->interp, "Current point is the first\n", (char *)NULL);
		return;
	    }
	    es_metaball_pnt = prev;
	    rt_metaball_pnt_print(es_metaball_pnt, s->dbip->dbi_base2local);
	    es_menu = arg;
	    es_edflag = IDLE;
	    sedit(s);
	    break;
	case MENU_METABALL_MOV_PT:
	    if (!es_metaball_pnt) {
		Tcl_AppendResult(s->interp, "No Metaball Point selected\n", (char *)NULL);
		es_edflag = IDLE;
		return;
	    }
	    es_menu = arg;
	    es_edflag = ECMD_METABALL_PT_MOV;
	    sedit(s);
	    break;
	case MENU_METABALL_PT_FLDSTR:
	    if (!es_metaball_pnt) {
		Tcl_AppendResult(s->interp, "No Metaball Point selected\n", (char *)NULL);
		es_edflag = IDLE;
		return;
	    }
	    es_menu = arg;
	    es_edflag = PSCALE;
	    break;
	case MENU_METABALL_DEL_PT:
	    es_menu = arg;
	    es_edflag = ECMD_METABALL_PT_DEL;
	    sedit(s);
	    break;
	case MENU_METABALL_ADD_PT:
	    es_menu = arg;
	    es_edflag = ECMD_METABALL_PT_ADD;
	    break;
    }
    set_e_axes_pos(s, 1);
    return;
}

struct menu_item metaball_menu[] = {
    { "METABALL MENU", NULL, 0 },
    { "Set Threshold", metaball_ed, MENU_METABALL_SET_THRESHOLD },
    { "Set Render Method", metaball_ed, MENU_METABALL_SET_METHOD },
    { "Select Point", metaball_ed, MENU_METABALL_SELECT },
    { "Next Point", metaball_ed, MENU_METABALL_NEXT_PT },
    { "Previous Point", metaball_ed, MENU_METABALL_PREV_PT },
    { "Move Point", metaball_ed, MENU_METABALL_MOV_PT },
    { "Scale Point fldstr", metaball_ed, MENU_METABALL_PT_FLDSTR },
    { "Scale Point \"goo\" value", metaball_ed, MENU_METABALL_PT_SET_GOO },
    { "Delete Point", metaball_ed, MENU_METABALL_DEL_PT },
    { "Add Point", metaball_ed, MENU_METABALL_ADD_PT },
    { "", NULL, 0 }
};

struct menu_item *
mged_metaball_menu_item(const struct bn_tol *UNUSED(tol))
{
    return metaball_menu;
}


void
mged_metaball_labels(
	int *UNUSED(num_lines),
	point_t *UNUSED(lines),
	struct rt_point_labels *pl,
	int UNUSED(max_pl),
	const mat_t xform,
	struct rt_db_internal *ip,
	struct bn_tol *UNUSED(tol))
{
    point_t pos_view;
    int npl = 0;

    RT_CK_DB_INTERNAL(ip);

#define POINT_LABEL_STR(_pt, _str) { \
    VMOVE(pl[npl].pt, _pt); \
    bu_strlcpy(pl[npl++].str, _str, sizeof(pl[0].str)); }

    struct rt_metaball_internal *metaball =
	(struct rt_metaball_internal *)ip->idb_ptr;

    RT_METABALL_CK_MAGIC(metaball);

    if (es_metaball_pnt) {
	BU_CKMAG(es_metaball_pnt, WDB_METABALLPT_MAGIC, "wdb_metaball_pnt");

	MAT4X3PNT(pos_view, xform, es_metaball_pnt->coord);
	POINT_LABEL_STR(pos_view, "pt");
    }

    pl[npl].str[0] = '\0';	/* Mark ending */
}

const char *
mged_metaball_keypoint(
	point_t *pt,
	const char *UNUSED(keystr),
	const mat_t mat,
	const struct rt_db_internal *ip,
	const struct bn_tol *UNUSED(tol))
{
    RT_CK_DB_INTERNAL(ip);
    point_t mpt = VINIT_ZERO;
    VSETALL(mpt, 0.0);
    static char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);

    struct rt_metaball_internal *metaball = (struct rt_metaball_internal *)ip->idb_ptr;
    RT_METABALL_CK_MAGIC(metaball);

    if (es_metaball_pnt==NULL) {
	snprintf(buf, BUFSIZ, "no point selected");
    } else {
	VMOVE(mpt, es_metaball_pnt->coord);
	snprintf(buf, BUFSIZ, "V %f", es_metaball_pnt->fldstr);
    }

    MAT4X3PNT(*pt, mat, mpt);
    return (const char *)buf;
}

int
menu_metaball_set_threshold(struct mged_state *s)
{
    if (es_para[0] < 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR < 0\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    struct rt_metaball_internal *ball =
	(struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;
    RT_METABALL_CK_MAGIC(ball);
    ball->threshold = es_para[0];

    return 0;
}

int
menu_metaball_set_method(struct mged_state *s)
{
    if (es_para[0] < 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR < 0\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    struct rt_metaball_internal *ball =
	(struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;
    RT_METABALL_CK_MAGIC(ball);
    ball->method = es_para[0];

    return 0;
}

int
menu_metaball_pt_set_goo(struct mged_state *s)
{
    if (es_para[0] < 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR < 0\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    if (!es_metaball_pnt || !inpara) {
	Tcl_AppendResult(s->interp, "pscale: no metaball point selected for scaling goo\n", (char *)NULL);
	return TCL_ERROR;
    }
    es_metaball_pnt->sweat *= *es_para * ((s->edit_state.es_scale > -SMALL_FASTF) ? s->edit_state.es_scale : 1.0);

    return 0;
}

int
menu_metaball_pt_fldstr(struct mged_state *s)
{
    if (es_para[0] <= 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    if (!es_metaball_pnt || !inpara) {
	Tcl_AppendResult(s->interp, "pscale: no metaball point selected for scaling strength\n", (char *)NULL);
	return TCL_ERROR;
    }

    es_metaball_pnt->fldstr *= *es_para * ((s->edit_state.es_scale > -SMALL_FASTF) ? s->edit_state.es_scale : 1.0);

    return 0;
}

void
ecmd_metaball_pt_pick(struct mged_state *s)
{
    struct rt_metaball_internal *metaball=
	(struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;
    point_t new_pt;
    struct wdb_metaball_pnt *ps;
    struct wdb_metaball_pnt *nearest=(struct wdb_metaball_pnt *)NULL;
    fastf_t min_dist = MAX_FASTF;
    vect_t dir, work;

    RT_METABALL_CK_MAGIC(metaball);

    if (es_mvalid) {
	VMOVE(new_pt, es_mparam);
    } else if (inpara == 3) {
	VMOVE(new_pt, es_para);
    } else if (inpara) {
	Tcl_AppendResult(s->interp, "x y z coordinates required for control point selection\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
	return;
    } else {
	return;
    }

    /* get a direction vector in model space corresponding to z-direction in view */
    VSET(work, 0.0, 0.0, 1.0);
    MAT4X3VEC(dir, view_state->vs_gvp->gv_view2model, work);

    for (BU_LIST_FOR(ps, wdb_metaball_pnt, &metaball->metaball_ctrl_head)) {
	fastf_t dist;

	dist = bg_dist_line3_pnt3(new_pt, dir, ps->coord);
	if (dist < min_dist) {
	    min_dist = dist;
	    nearest = ps;
	}
    }

    es_metaball_pnt = nearest;

    if (!es_metaball_pnt) {
	Tcl_AppendResult(s->interp, "No METABALL control point selected\n", (char *)NULL);
	mged_print_result(s, TCL_ERROR);
    } else {
	rt_metaball_pnt_print(es_metaball_pnt, s->dbip->dbi_base2local);
    }
}

void
ecmd_metaball_pt_mov(struct mged_state *UNUSED(s))
{
    if (!es_metaball_pnt) {
	bu_log("Must select a point to move"); return; }
    if (inpara != 3) {
	bu_log("Must provide dx dy dz"); return; }
    VADD2(es_metaball_pnt->coord, es_metaball_pnt->coord, es_para);
}

void
ecmd_metaball_pt_del(struct mged_state *UNUSED(s))
{
    struct wdb_metaball_pnt *tmp = es_metaball_pnt, *p;

    if (es_metaball_pnt == NULL) {
	bu_log("No point selected");
	return;
    }
    p = BU_LIST_PREV(wdb_metaball_pnt, &es_metaball_pnt->l);
    if (p->l.magic == BU_LIST_HEAD_MAGIC) {
	es_metaball_pnt = BU_LIST_NEXT(wdb_metaball_pnt, &es_metaball_pnt->l);
	/* 0 point metaball... allow it for now. */
	if (es_metaball_pnt->l.magic == BU_LIST_HEAD_MAGIC)
	    es_metaball_pnt = NULL;
    } else
	es_metaball_pnt = p;
    BU_LIST_DQ(&tmp->l);
    free(tmp);
    if (!es_metaball_pnt)
	bu_log("WARNING: Last point of this metaball has been deleted.");
}

void
ecmd_metaball_pt_add(struct mged_state *s)
{
    struct rt_metaball_internal *metaball= (struct rt_metaball_internal *)s->edit_state.es_int.idb_ptr;
    struct wdb_metaball_pnt *n = (struct wdb_metaball_pnt *)malloc(sizeof(struct wdb_metaball_pnt));

    if (inpara != 3) {
	bu_log("Must provide x y z");
	bu_free(n, "wdb_metaball_pnt n");
	return;
    }

    es_metaball_pnt = BU_LIST_FIRST(wdb_metaball_pnt, &metaball->metaball_ctrl_head);
    VMOVE(n->coord, es_para);
    n->l.magic = WDB_METABALLPT_MAGIC;
    n->fldstr = 1.0;
    BU_LIST_APPEND(&es_metaball_pnt->l, &n->l);
    es_metaball_pnt = n;
}

static int
mged_metaball_pscale(struct mged_state *s, int mode)
{
    if (inpara > 1) {
	Tcl_AppendResult(s->interp, "ERROR: only one argument needed\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    if (es_para[0] <= 0.0) {
	Tcl_AppendResult(s->interp, "ERROR: SCALE FACTOR <= 0\n", (char *)NULL);
	inpara = 0;
	return TCL_ERROR;
    }

    switch (mode) {
	case MENU_METABALL_SET_THRESHOLD:
	    return menu_metaball_set_threshold(s);
	case MENU_METABALL_SET_METHOD:
	    return menu_metaball_set_method(s);
	case MENU_METABALL_PT_SET_GOO:
	    return menu_metaball_pt_set_goo(s);
	case MENU_METABALL_PT_FLDSTR:
	    return menu_metaball_pt_fldstr(s);
    };

    return 0;
}

void
mged_metaball_edit(struct mged_state *s, int edflag)
{
    switch (edflag) {
	case SSCALE:
	    /* scale the solid uniformly about its vertex point */
	    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
	    mged_generic_sscale(s, &s->edit_state.es_int);
	    break;
	case STRANS:
	    /* translate solid */
	    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
	    mged_generic_strans(s, &s->edit_state.es_int);
	    break;
	case SROT:
	    /* rot solid about vertex */
	    es_metaball_pnt = (struct wdb_metaball_pnt *)NULL; /* Reset es_metaball_pnt */
	    mged_generic_srot(s, &s->edit_state.es_int);
	    break;
	case PSCALE:
	    mged_metaball_pscale(s, es_menu);
	    break;
	case ECMD_METABALL_PT_PICK:
	    ecmd_metaball_pt_pick(s);
	    break;
	case ECMD_METABALL_PT_MOV:
	    ecmd_metaball_pt_mov(s);
	    break;
	case ECMD_METABALL_PT_DEL:
	    ecmd_metaball_pt_del(s);
	    break;
	case ECMD_METABALL_PT_ADD:
	    ecmd_metaball_pt_add(s);
	    break;
    }
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
