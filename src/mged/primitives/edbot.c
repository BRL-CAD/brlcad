/*                         E D B O T . C
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
/** @file mged/primitives/edbot.c
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
#include "./edbot.h"

static void
bot_ed(struct mged_state *s, int arg, int UNUSED(a), int UNUSED(b))
{
    es_menu = arg;
    es_edflag = arg;

    sedit(s);
    set_e_axes_pos(s, 1);
}
struct menu_item bot_menu[] = {
    { "BOT MENU", NULL, 0 },
    { "Pick Vertex", bot_ed, ECMD_BOT_PICKV },
    { "Pick Edge", bot_ed, ECMD_BOT_PICKE },
    { "Pick Triangle", bot_ed, ECMD_BOT_PICKT },
    { "Move Vertex", bot_ed, ECMD_BOT_MOVEV },
    { "Move Edge", bot_ed, ECMD_BOT_MOVEE },
    { "Move Triangle", bot_ed, ECMD_BOT_MOVET },
    { "Delete Triangle", bot_ed, ECMD_BOT_FDEL },
    { "Select Mode", bot_ed, ECMD_BOT_MODE },
    { "Select Orientation", bot_ed, ECMD_BOT_ORIENT },
    { "Set flags", bot_ed, ECMD_BOT_FLAGS },
    { "Set Face Thickness", bot_ed, ECMD_BOT_THICK },
    { "Set Face Mode", bot_ed, ECMD_BOT_FMODE },
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
