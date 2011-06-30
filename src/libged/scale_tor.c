/*                         S C A L E _ T O R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/scale_tor.c
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
_ged_scale_tor(struct ged *gedp, struct rt_tor_internal *tor, const char *attribute, fastf_t sf, int rflag)
{
    fastf_t newrad;

    RT_TOR_CK_MAGIC(tor);

    switch (attribute[0]) {
	case 'a':
	case 'A':
	    if (rflag)
		newrad = tor->r_a * sf;
	    else
		newrad = sf;

	    if (newrad < SMALL)
		newrad = 4*SMALL;
	    if (tor->r_h <= newrad)
		tor->r_a = newrad;
	    break;
	case 'h':
	case 'H':
	    if (rflag)
		newrad = tor->r_h * sf;
	    else
		newrad = sf;

	    if (newrad < SMALL)
		newrad = 4*SMALL;
	    if (newrad <= tor->r_a)
		tor->r_h = newrad;
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad tor attribute - %s", attribute);
	    return GED_ERROR;
    }

    return GED_OK;
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
