/*                         S C A L E _ E L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file scale_ell.c
 *
 * The scale_ell command.
 *
 * FIXME: This command really probably shouldn't exist.  The way MGED
 * currently handles scaling is to pass a transformation matrix to
 * transform_editing_solid(), which simply calls
 * rt_matrix_transform().  The primitives already know how to apply an
 * arbitrary matrix transform to their data making the need for
 * primitive-specific editing commands such as this one unnecessary.
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
ged_scale_ell(struct ged *gedp, struct rt_ell_internal *ell, char *attribute, fastf_t sf)
{
    fastf_t ma, mb;

    RT_ELL_CK_MAGIC(ell);

    switch (attribute[0]) {
    case 'a':
    case 'A':
	VSCALE(ell->a, ell->a, sf);
	break;
    case 'b':
    case 'B':
	VSCALE(ell->b, ell->b, sf);
	break;
    case 'c':
    case 'C':
	VSCALE(ell->c, ell->c, sf);
	break;
    case '3':
	VSCALE(ell->a, ell->a, sf);
	VSCALE(ell->b, ell->b, sf);
	VSCALE(ell->c, ell->c, sf);
	break;
    default:
	bu_vls_printf(&gedp->ged_result_str, "bad ell attribute - %s", attribute);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
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
