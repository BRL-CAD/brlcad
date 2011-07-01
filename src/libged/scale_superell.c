/*                         S C A L E _ S U P E R E L L . C
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
/** @file libged/scale_superell.c
 *
 * The scale_superell command.
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"

int
_ged_scale_superell(struct ged *gedp, struct rt_superell_internal *superell, const char *attribute, fastf_t sf, int rflag)
{
    fastf_t ma, mb;

    RT_SUPERELL_CK_MAGIC(superell);

    switch (attribute[0]) {
	case 'a':
	case 'A':
	    if (!rflag)
		sf /= MAGNITUDE(superell->a);

	    switch (attribute[1]) {
		case '\0':
		    VSCALE(superell->a, superell->a, sf);
		    break;
		case 'b':
		case 'B':
		    if ((attribute[2] == 'c' || attribute[2] == 'C') &&
			attribute[3] == '\0') {
			/* set A, B, and C lengths the same */
			VSCALE(superell->a, superell->a, sf);
			ma = MAGNITUDE(superell->a);
			mb = MAGNITUDE(superell->b);
			VSCALE(superell->b, superell->b, ma/mb);
			mb = MAGNITUDE(superell->c);
			VSCALE(superell->c, superell->c, ma/mb);
		    } else {
			bu_vls_printf(gedp->ged_result_str, "bad ell attribute - %s", attribute);
			return GED_ERROR;
		    }

		    break;
		default:
		    bu_vls_printf(gedp->ged_result_str, "bad ell attribute - %s", attribute);
		    return GED_ERROR;
	    }

	    break;
	case 'b':
	case 'B':
	    if (!rflag)
		sf /= MAGNITUDE(superell->b);

	    VSCALE(superell->b, superell->b, sf);
	    break;
	case 'c':
	case 'C':
	    if (!rflag)
		sf /= MAGNITUDE(superell->c);

	    VSCALE(superell->c, superell->c, sf);
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad superell attribute - %s", attribute);
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
