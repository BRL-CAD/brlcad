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
#include "./edmetaball.h"

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



/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
