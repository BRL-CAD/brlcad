/*                         T R A N S L A T E _ T G C . C
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
/** @file libged/translate_tgc.c
 *
 * The translate_tgc command.
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
_ged_translate_tgc(struct ged *gedp, struct rt_tgc_internal *tgc, const char *attribute, vect_t tvec, int rflag)
{
    fastf_t la, lb, lc, ld;
    vect_t hvec;

    RT_TGC_CK_MAGIC(tgc);

    VSCALE(tvec, tvec, gedp->ged_wdbp->dbip->dbi_local2base);

    switch (attribute[0]) {
	case 'h':
	case 'H':
	    switch (attribute[1]) {
		case '\0':
		    if (rflag) {
			VADD2(hvec, tgc->h, tvec);
		    } else {
			VSUB2(hvec, tvec, tgc->v);
		    }

		    /* check for zero H vector */
		    if (MAGNITUDE(hvec) <= SQRT_SMALL_FASTF) {
			bu_vls_printf(gedp->ged_result_str, "Zero H vector not allowed.");
			return GED_ERROR;
		    }

		    VMOVE(tgc->h, hvec);

		    /* have new height vector -- redefine rest of tgc */
		    la = MAGNITUDE(tgc->a);
		    lb = MAGNITUDE(tgc->b);
		    lc = MAGNITUDE(tgc->c);
		    ld = MAGNITUDE(tgc->d);

		    /* find 2 perpendicular vectors normal to H for new A, B */
		    bn_vec_perp(tgc->b, tgc->h);
		    VCROSS(tgc->a, tgc->b, tgc->h);
		    VUNITIZE(tgc->a);
		    VUNITIZE(tgc->b);

		    /* Create new C, D from unit length A, B, with previous len */
		    VSCALE(tgc->c, tgc->a, lc);
		    VSCALE(tgc->d, tgc->b, ld);

		    /* Restore original vector lengths to A, B */
		    VSCALE(tgc->a, tgc->a, la);
		    VSCALE(tgc->b, tgc->b, lb);

		    break;
		case 'h':
		case 'H':
		    if (attribute[2] != '\0') {
			bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
			return GED_ERROR;
		    }

		    if (rflag) {
			VADD2(hvec, tgc->h, tvec);
		    } else {
			VSUB2(hvec, tvec, tgc->v);
		    }

		    /* check for zero H vector */
		    if (MAGNITUDE(hvec) <= SQRT_SMALL_FASTF) {
			bu_vls_printf(gedp->ged_result_str, "Zero H vector not allowed.");
			return GED_ERROR;
		    }

		    VMOVE(tgc->h, hvec);

		    break;
		default:
		    bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
		    return GED_ERROR;
	    }

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
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
