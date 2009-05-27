/*                         S C A L E _ R H C . C
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
/** @file scale_rhc.c
 *
 * The scale_rhc command.
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"

int
ged_scale_rhc(struct ged *gedp, struct rt_rhc_internal *rhc, const char *attribute, fastf_t sf)
{
    fastf_t ma, mb;

    RT_RHC_CK_MAGIC(rhc);

    switch (attribute[0]) {
    case 'b':
    case 'B':
	VSCALE(rhc->rhc_B, rhc->rhc_B, sf);
	break;
    case 'h':
    case 'H':
	VSCALE(rhc->rhc_H, rhc->rhc_H, sf);
	break;
    case 'c':
    case 'C':
	rhc->rhc_c *= sf;
	break;
    case 'r':
    case 'R':
	rhc->rhc_r *= sf;
	break;
    default:
	bu_vls_printf(&gedp->ged_result_str, "bad rhc attribute - %s", attribute);
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
