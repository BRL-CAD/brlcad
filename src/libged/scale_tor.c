/*                         S C A L E _ T O R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file scale_tor.c
 *
 * The scale_tor command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"


int
ged_scale_tor(struct ged *gedp, struct rt_tor_internal *tor, char *attribute, fastf_t sf)
{
    RT_TOR_CK_MAGIC(tor);

    switch (attribute[0]) {
    case 'a':
    case 'A':
	tor->r_a *= sf;
	if (tor->r_a < SMALL)
	    tor->r_a = 4*SMALL;
	break;
    case 'h':
    case 'H':
	tor->r_h *= sf;
	if (tor->r_h < SMALL)
	    tor->r_h = 4*SMALL;
	break;
    default:
	bu_vls_printf(&gedp->ged_result_str, "bad tor attribute - %s", attribute);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
