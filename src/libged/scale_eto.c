/*                         S C A L E _ E T O . C
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
/** @file libged/scale_eto.c
 *
 * The scale_eto command.
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
_ged_scale_eto(struct ged *gedp, struct rt_eto_internal *eto, const char *attribute, fastf_t sf, int rflag)
{
    RT_ETO_CK_MAGIC(eto);

    switch (attribute[0]) {
	case 'r':
	case 'R': {
	    fastf_t ch, cv, dh, newrad;
	    vect_t Nu;

	    if (rflag)
		newrad = eto->eto_r * sf;
	    else
		newrad = sf;

	    if (newrad < SMALL)
		newrad = 4*SMALL;

	    VMOVE(Nu, eto->eto_N);
	    VUNITIZE(Nu);

	    /* get horiz and vert components of C and Rd */
	    cv = VDOT(eto->eto_C, Nu);
	    ch = sqrt(VDOT(eto->eto_C, eto->eto_C) - cv * cv);

	    /* angle between C and Nu */
	    dh = eto->eto_rd * cv / MAGNITUDE(eto->eto_C);

	    /* make sure revolved ellipse doesn't overlap itself */
	    if (ch <= newrad && dh <= newrad)
		eto->eto_r = newrad;
	}
	    break;
	case 'd':
	case 'D': {
	    fastf_t dh, newrad, work;
	    vect_t Nu;

	    if (rflag)
		newrad = eto->eto_rd * sf;
	    else
		newrad = sf;

	    if (newrad < SMALL)
		newrad = 4*SMALL;
	    work = MAGNITUDE(eto->eto_C);
	    if (newrad <= work) {
		VMOVE(Nu, eto->eto_N);
		VUNITIZE(Nu);
		dh = newrad * VDOT(eto->eto_C, Nu) / work;
		/* make sure revolved ellipse doesn't overlap itself */
		if (dh <= eto->eto_r)
		    eto->eto_rd = newrad;
	    }
	}
	    break;
	case 'c':
	case 'C': {
	    fastf_t ch, cv;
	    vect_t Nu, Work;

	    if (!rflag)
		sf /= MAGNITUDE(eto->eto_C);

	    if (sf * MAGNITUDE(eto->eto_C) >= eto->eto_rd) {
		VMOVE(Nu, eto->eto_N);
		VUNITIZE(Nu);
		VSCALE(Work, eto->eto_C, sf);

		/* get horiz and vert comps of C and Rd */
		cv = VDOT(Work, Nu);
		ch = sqrt(VDOT(Work, Work) - cv * cv);
		if (ch <= eto->eto_r)
		    VMOVE(eto->eto_C, Work);
	    }
	}
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad eto attribute - %s", attribute);
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
