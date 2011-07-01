/*                         S C A L E _ E P A . C
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
/** @file libged/scale_epa.c
 *
 * The scale_epa command.
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"

int
_ged_scale_epa(struct ged *gedp, struct rt_epa_internal *epa, const char *attribute, fastf_t sf, int rflag)
{
    fastf_t newrad;

    RT_EPA_CK_MAGIC(epa);

    switch (attribute[0]) {
	case 'h':
	case 'H':
	    if (!rflag)
		sf /= MAGNITUDE(epa->epa_H);

	    VSCALE(epa->epa_H, epa->epa_H, sf);
	    break;
	case 'a':
	case 'A':
	    if (rflag)
		newrad = epa->epa_r1 * sf;
	    else
		newrad = sf;

	    if (newrad >= epa->epa_r2)
		epa->epa_r1 = newrad;
	    break;
	case 'b':
	case 'B':
	    if (rflag)
		newrad = epa->epa_r2 * sf;
	    else
		newrad = sf;

	    if (newrad <= epa->epa_r1)
		epa->epa_r2 = newrad;
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad epa attribute - %s", attribute);
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
