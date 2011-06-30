/*                      R O T A T E _ E T O . C
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
/** @file libged/rotate_eto.c
 *
 * The rotate_eto command.
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
_ged_rotate_eto(struct ged *gedp, struct rt_eto_internal *eto, const char *attribute, matp_t rmat)
{
    RT_ETO_CK_MAGIC(eto);

    if (attribute[1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "bad eto attribute - %s", attribute);
	return GED_ERROR;
    }

    switch (attribute[0]) {
	case 'c':
	case 'C':
	    MAT4X3VEC(eto->eto_C, rmat, eto->eto_C);
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
