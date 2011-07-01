/*                         S C A L E _ E H Y . C
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
/** @file libged/scale_ehy.c
 *
 * The scale_ehy command.
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"

int
_ged_scale_ehy(struct ged *gedp, struct rt_ehy_internal *ehy, const char *attribute, fastf_t sf, int rflag)
{
    fastf_t newrad;

    RT_EHY_CK_MAGIC(ehy);

    switch (attribute[0]) {
	case 'h':
	case 'H':
	    if (!rflag)
		sf /= MAGNITUDE(ehy->ehy_H);

	    VSCALE(ehy->ehy_H, ehy->ehy_H, sf);
	    break;
	case 'a':
	case 'A':
	    if (rflag)
		newrad = ehy->ehy_r1 * sf;
	    else
		newrad = sf;

	    if (newrad >= ehy->ehy_r2)
		ehy->ehy_r1 = newrad;
	    break;
	case 'b':
	case 'B':
	    if (rflag)
		newrad = ehy->ehy_r2 * sf;
	    else
		newrad = sf;

	    if (newrad <= ehy->ehy_r1)
		ehy->ehy_r2 = newrad;
	    break;
	case 'c':
	case 'C':
	    if (rflag)
		ehy->ehy_c *= sf;
	    else
		ehy->ehy_c = sf;

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad ehy attribute - %s", attribute);
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
