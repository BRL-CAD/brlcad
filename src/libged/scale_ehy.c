/*                         S C A L E _ E H Y . C
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
ged_scale_ehy(struct ged *gedp, struct rt_ehy_internal *ehy, char *attribute, fastf_t sf)
{
    fastf_t ma, mb;

    RT_EHY_CK_MAGIC(ehy);

    switch (attribute[0]) {
    case 'h':
    case 'H':
	VSCALE(ehy->ehy_H, ehy->ehy_H, sf);
	break;
    case 'a':
    case 'A':
	if (ehy->ehy_r1 * sf >= ehy->ehy_r2)
	    ehy->ehy_r1 *= sf;
	break;
    case 'b':
    case 'B':
	if (ehy->ehy_r2 * sf <= ehy->ehy_r1)
	    ehy->ehy_r2 *= sf;
	break;
    case 'c':
    case 'C':
	ehy->ehy_c *= sf;
	break;
    default:
	bu_vls_printf(&gedp->ged_result_str, "bad ehy attribute - %s", attribute);
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
