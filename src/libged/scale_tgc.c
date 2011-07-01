/*                         S C A L E _ T G C . C
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
/** @file libged/scale_tgc.c
 *
 * The scale_tgc command.
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
_ged_scale_tgc(struct ged *gedp, struct rt_tgc_internal *tgc, const char *attribute, fastf_t sf, int rflag)
{
    fastf_t ma, mb;

    RT_TGC_CK_MAGIC(tgc);

    switch (attribute[0]) {
	case 'a':
	case 'A':
	    if (!rflag)
		sf /= MAGNITUDE(tgc->a);

	    switch (attribute[1]) {
		case '\0':
		    VSCALE(tgc->a, tgc->a, sf);
		    break;
		case 'b':
		case 'B':
		    switch (attribute[2]) {
			case '\0':
			    VSCALE(tgc->a, tgc->a, sf);
			    ma = MAGNITUDE(tgc->a);
			    mb = MAGNITUDE(tgc->b);
			    VSCALE(tgc->b, tgc->b, ma/mb);

			    break;
			case 'c':
			case 'C':
			    if ((attribute[3] == 'd' || attribute[3] == 'D') &&
				attribute[4] == '\0') {
				VSCALE(tgc->a, tgc->a, sf);
				ma = MAGNITUDE(tgc->a);
				mb = MAGNITUDE(tgc->b);
				VSCALE(tgc->b, tgc->b, ma/mb);
				mb = MAGNITUDE(tgc->c);
				VSCALE(tgc->c, tgc->c, ma/mb);
				mb = MAGNITUDE(tgc->d);
				VSCALE(tgc->d, tgc->d, ma/mb);
			    } else {
				bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
				return GED_ERROR;
			    }

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

	    break;
	case 'b':
	case 'B':
	    if (!rflag)
		sf /= MAGNITUDE(tgc->b);

	    VSCALE(tgc->b, tgc->b, sf);
	    break;
	case 'c':
	case 'C':
	    if (!rflag)
		sf /= MAGNITUDE(tgc->c);

	    switch (attribute[1]) {
		case '\0':
		    VSCALE(tgc->c, tgc->c, sf);
		    break;
		case 'd':
		case 'D':
		    VSCALE(tgc->c, tgc->c, sf);
		    ma = MAGNITUDE(tgc->c);
		    mb = MAGNITUDE(tgc->d);
		    VSCALE(tgc->d, tgc->d, ma/mb);
		    break;
		default:
		    bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
		    return GED_ERROR;
	    }

	    break;
	case 'd':
	case 'D':
	    if (!rflag)
		sf /= MAGNITUDE(tgc->d);

	    VSCALE(tgc->d, tgc->d, sf);
	    break;
	case 'h':
	case 'H':
	    if (!rflag)
		sf /= MAGNITUDE(tgc->h);

	    switch (attribute[1]) {
		case '\0':
		    VSCALE(tgc->h, tgc->h, sf);
		    break;
		case 'c':
		case 'C':
		    if ((attribute[2] == 'd' || attribute[2] == 'D') &&
			attribute[3] == '\0') {
			vect_t vec1, vec2;
			vect_t c, d;

			/* calculate new c */
			VSUB2(vec1, tgc->a, tgc->c);
			VSCALE(vec2, vec1, 1-sf);
			VADD2(c, tgc->c, vec2);

			/* calculate new d */
			VSUB2(vec1, tgc->b, tgc->d);
			VSCALE(vec2, vec1, 1-sf);
			VADD2(d, tgc->d, vec2);

			if (0 <= VDOT(tgc->c, c) &&
			    0 <= VDOT(tgc->d, d) &&
			    !ZERO(MAGNITUDE(c)) &&
			    !ZERO(MAGNITUDE(d))) {
			    /* adjust c, d and h */
			    VMOVE(tgc->c, c);
			    VMOVE(tgc->d, d);
			    VSCALE(tgc->h, tgc->h, sf);
			}
		    } else {
			bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
			return GED_ERROR;
		    }

		    break;
		case 'v':
		case 'V':
		    switch (attribute[2]) {
			case '\0': {
			    point_t old_top;

			    VADD2(old_top, tgc->v, tgc->h);
			    VSCALE(tgc->h, tgc->h, sf);
			    VSUB2(tgc->v, old_top, tgc->h);
			}

			    break;
			case 'a':
			case 'A':
			    if ((attribute[3] == 'b' || attribute[3] == 'B') &&
				attribute[4] == '\0') {
				vect_t vec1, vec2;
				vect_t a, b;
				point_t old_top;

				/* calculate new a */
				VSUB2(vec1, tgc->c, tgc->a);
				VSCALE(vec2, vec1, 1-sf);
				VADD2(a, tgc->a, vec2);

				/* calculate new b */
				VSUB2(vec1, tgc->d, tgc->b);
				VSCALE(vec2, vec1, 1-sf);
				VADD2(b, tgc->b, vec2);

				if (0 <= VDOT(tgc->a, a) &&
				    0 <= VDOT(tgc->b, b) &&
				    !ZERO(MAGNITUDE(a)) &&
				    !ZERO(MAGNITUDE(b))) {
				    /* adjust a, b, v and h */
				    VMOVE(tgc->a, a);
				    VMOVE(tgc->b, b);
				    VADD2(old_top, tgc->v, tgc->h);
				    VSCALE(tgc->h, tgc->h, sf);
				    VSUB2(tgc->v, old_top, tgc->h);
				}
			    } else {
				bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
				return GED_ERROR;
			    }

			    break;
			default:
			    bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
			    return GED_ERROR;
		    } /* switch (attribute[2]) */

		    break;
		default:
		    bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
		    return GED_ERROR;
	    } /* switch (attribute[1]) */

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad tgc attribute - %s", attribute);
	    return GED_ERROR;
    } /* switch (attribute[0]) */

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
