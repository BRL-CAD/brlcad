/*                         R O T A T E _ T G C . C
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
/** @file libged/rotate_tgc.c
 *
 * The rotate_tgc command.
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
_ged_rotate_tgc(struct ged *gedp, struct rt_tgc_internal *tgc, const char *attribute, matp_t rmat)
{
    RT_TGC_CK_MAGIC(tgc);

    switch (attribute[0]) {
	case 'h':
	case 'H':
	    switch (attribute[1]) {
		case '\0':
		    MAT4X3VEC(tgc->h, rmat, tgc->h);
		    break;
		case 'a':
		case 'A':
		    if ((attribute[2] == 'b' || attribute[2] == 'B') &&
			attribute[3] == '\0') {
			MAT4X3VEC(tgc->a, rmat, tgc->a);
			MAT4X3VEC(tgc->b, rmat, tgc->b);
			MAT4X3VEC(tgc->c, rmat, tgc->c);
			MAT4X3VEC(tgc->d, rmat, tgc->d);
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
