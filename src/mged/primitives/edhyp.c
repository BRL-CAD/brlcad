/*                         E D H Y P . C
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
/** @file mged/primitives/edhyp.c
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
#include "./edhyp.h"

static void
hyp_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    switch (arg) {
	case MENU_HYP_ROT_H:
	    es_edflag = ECMD_HYP_ROT_H;
	    break;
	default:
	    es_edflag = PSCALE;
	    break;
    }
    set_e_axes_pos(s, 1);
    return;
}
struct menu_item  hyp_menu[] = {
    { "HYP MENU", NULL, 0 },
    { "Set H", hyp_ed, MENU_HYP_H },
    { "Set A", hyp_ed, MENU_HYP_SCALE_A },
    { "Set B", hyp_ed, MENU_HYP_SCALE_B },
    { "Set c", hyp_ed, MENU_HYP_C },
    { "Rotate H", hyp_ed, MENU_HYP_ROT_H },
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
