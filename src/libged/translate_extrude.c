/*                         T R A N S L A T E _ E X T R U D E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/translate_extrude.c
 *
 * The translate_extrude command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "bu/cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"


int
_ged_translate_extrude(struct ged *gedp, struct rt_extrude_internal *extrude, const char *attribute, vect_t tvec, int rflag)
{
    vect_t hvec;

    RT_EXTRUDE_CK_MAGIC(extrude);

    VSCALE(tvec, tvec, gedp->ged_wdbp->dbip->dbi_local2base);

    switch (attribute[0]) {
	case 'h':
	case 'H':
	    switch (attribute[1]) {
	    case '\0':
		if (rflag) {
		    VADD2(hvec, extrude->h, tvec);
		} else {
		    VSUB2(hvec, tvec, extrude->V);
		}

		/* check for zero H vector */
		if (MAGNITUDE(hvec) <= SQRT_SMALL_FASTF) {
		    bu_vls_printf(gedp->ged_result_str, "Zero H vector not allowed.");
		    return GED_ERROR;
		}

		VMOVE(extrude->h, hvec);

		break;
	    case 'r':
	    case 'R':
		if (attribute[2] != '\0') {
		    bu_vls_printf(gedp->ged_result_str, "bad extrude attribute - %s", attribute);
		    return GED_ERROR;
		}

		if (rflag) {
		    VADD2(hvec, extrude->h, tvec);
		} else {
		    VSUB2(hvec, tvec, extrude->V);
		}

		/* check for zero H vector */
		if (MAGNITUDE(hvec) <= SQRT_SMALL_FASTF) {
		    bu_vls_printf(gedp->ged_result_str, "Zero H vector not allowed.");
		    return GED_ERROR;
		}

		VMOVE(extrude->h, hvec);

		/* Cross h with the existing u_vec to insure that the new v_vec is perpendicular to h */
		VCROSS(extrude->v_vec, extrude->h, extrude->u_vec);

		/* Cross v_vec with h to insure that the new u_vec is perpendicular to h as well as v_vec */
		VCROSS(extrude->u_vec, extrude->v_vec, extrude->h);

		VUNITIZE(extrude->v_vec);
		VUNITIZE(extrude->u_vec);

		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "bad extrude attribute - %s", attribute);
		return GED_ERROR;
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
