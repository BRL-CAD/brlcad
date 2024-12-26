/*                         E D E B M . C
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
/** @file mged/primitives/edebm.c
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
#include "./edebm.h"

static void
ebm_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;

    switch (arg) {
	case MENU_EBM_FNAME:
	    es_edflag = ECMD_EBM_FNAME;
	    break;
	case MENU_EBM_FSIZE:
	    es_edflag = ECMD_EBM_FSIZE;
	    break;
	case MENU_EBM_HEIGHT:
	    es_edflag = ECMD_EBM_HEIGHT;
	    break;
    }

    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item ebm_menu[] = {
    {"EBM MENU", NULL, 0 },
    {"File Name", ebm_ed, MENU_EBM_FNAME },
    {"File Size (W N)", ebm_ed, MENU_EBM_FSIZE },
    {"Extrude Depth", ebm_ed, MENU_EBM_HEIGHT },
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
