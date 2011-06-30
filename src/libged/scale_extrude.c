/*                         S C A L E _ E X T R U D E . C
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
/** @file libged/scale_extrude.c
 *
 * The scale_extrude command.
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
_ged_scale_extrude(struct ged *gedp, struct rt_extrude_internal *extrude, const char *attribute, fastf_t sf, int rflag)
{
    vect_t hvec;

    RT_EXTRUDE_CK_MAGIC(extrude);

    switch (attribute[0]) {
	case 'h':
	case 'H':
	    if (!rflag)
		sf /= MAGNITUDE(extrude->h);

	    VSCALE(hvec, extrude->h, sf);

	    /* Make sure hvec is not zero length */
	    if (MAGNITUDE(hvec) > SQRT_SMALL_FASTF) {
		VMOVE(extrude->h, hvec);
	    }

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad extrude attribute - %s", attribute);
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
