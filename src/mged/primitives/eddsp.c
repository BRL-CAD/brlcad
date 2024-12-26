/*                         E D D S P . C
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
/** @file mged/primitives/edcline.c
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
#include "./edcline.h"

static void
dsp_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;

    switch (arg) {
	case MENU_DSP_FNAME:
	    es_edflag = ECMD_DSP_FNAME;
	    break;
	case MENU_DSP_FSIZE:
	    es_edflag = ECMD_DSP_FSIZE;
	    break;
	case MENU_DSP_SCALE_X:
	    es_edflag = ECMD_DSP_SCALE_X;
	    break;
	case MENU_DSP_SCALE_Y:
	    es_edflag = ECMD_DSP_SCALE_Y;
	    break;
	case MENU_DSP_SCALE_ALT:
	    es_edflag = ECMD_DSP_SCALE_ALT;
	    break;
    }
    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item dsp_menu[] = {
    {"DSP MENU", NULL, 0 },
    {"Name", dsp_ed, MENU_DSP_FNAME },
    {"Set X", dsp_ed, MENU_DSP_SCALE_X },
    {"Set Y", dsp_ed, MENU_DSP_SCALE_Y },
    {"Set ALT", dsp_ed, MENU_DSP_SCALE_ALT },
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
