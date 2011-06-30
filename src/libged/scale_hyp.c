/*                         S C A L E _ H Y P . C
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
/** @file libged/scale_hyp.c
 *
 * The scale_hyp command.
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
_ged_scale_hyp(struct ged *gedp, struct rt_hyp_internal *hyp, const char *attribute, fastf_t sf, int rflag)
{
    fastf_t f;
    point_t old_top;

    RT_HYP_CK_MAGIC(hyp);

    switch (attribute[0]) {
	case 'h':
	case 'H':
	    if (!rflag)
		sf /= MAGNITUDE(hyp->hyp_Hi);

	    switch (attribute[1]) {
		case '\0':
		    VSCALE(hyp->hyp_Hi, hyp->hyp_Hi, sf);
		    break;
		case 'v':
		case 'V':
		    /* Scale H move V */
		    VADD2(old_top, hyp->hyp_Vi, hyp->hyp_Hi);
		    VSCALE(hyp->hyp_Hi, hyp->hyp_Hi, sf);
		    VSUB2(hyp->hyp_Vi, old_top, hyp->hyp_Hi);

		    break;
		default:
		    bu_vls_printf(gedp->ged_result_str, "bad hyp attribute - %s", attribute);
		    return GED_ERROR;
	    }
	    break;
	case 'a':
	case 'A':
	    if (!rflag)
		sf /= MAGNITUDE(hyp->hyp_A);

	    VSCALE(hyp->hyp_A, hyp->hyp_A, sf);
	    break;
	case 'b':
	case 'B':
	    if (rflag)
		hyp->hyp_b *= sf;
	    else
		hyp->hyp_b = sf;

	    break;
	case 'c':
	case 'C':
	    if (rflag)
		f = hyp->hyp_bnr * sf;
	    else
		f = sf;

	    if (f <= 1.0)
		hyp->hyp_bnr = f;

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad hyp attribute - %s", attribute);
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
