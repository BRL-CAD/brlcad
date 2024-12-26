/*                         E D T G C . C
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
/** @file mged/primitives/edtgc.c
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
#include "./edtgc.h"

static void
tgc_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = PSCALE;
    if (arg == MENU_TGC_ROT_H)
	es_edflag = ECMD_TGC_ROT_H;
    if (arg == MENU_TGC_ROT_AB)
	es_edflag = ECMD_TGC_ROT_AB;
    if (arg == MENU_TGC_MV_H)
	es_edflag = ECMD_TGC_MV_H;
    if (arg == MENU_TGC_MV_HH)
	es_edflag = ECMD_TGC_MV_HH;

    set_e_axes_pos(s, 1);
}

struct menu_item tgc_menu[] = {
    { "TGC MENU", NULL, 0 },
    { "Set H",	tgc_ed, MENU_TGC_SCALE_H },
    { "Set H (move V)", tgc_ed, MENU_TGC_SCALE_H_V },
    { "Set H (adj C,D)",	tgc_ed, MENU_TGC_SCALE_H_CD },
    { "Set H (move V, adj A,B)", tgc_ed, MENU_TGC_SCALE_H_V_AB },
    { "Set A",	tgc_ed, MENU_TGC_SCALE_A },
    { "Set B",	tgc_ed, MENU_TGC_SCALE_B },
    { "Set C",	tgc_ed, MENU_TGC_SCALE_C },
    { "Set D",	tgc_ed, MENU_TGC_SCALE_D },
    { "Set A,B",	tgc_ed, MENU_TGC_SCALE_AB },
    { "Set C,D",	tgc_ed, MENU_TGC_SCALE_CD },
    { "Set A,B,C,D", tgc_ed, MENU_TGC_SCALE_ABCD },
    { "Rotate H",	tgc_ed, MENU_TGC_ROT_H },
    { "Rotate AxB",	tgc_ed, MENU_TGC_ROT_AB },
    { "Move End H(rt)", tgc_ed, MENU_TGC_MV_H },
    { "Move End H", tgc_ed, MENU_TGC_MV_HH },
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
