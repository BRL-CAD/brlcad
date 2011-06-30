/*                         S C A L E _ R P C . C
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
/** @file libged/scale_rpc.c
 *
 * The scale_rpc command.
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"

int
_ged_scale_rpc(struct ged *gedp, struct rt_rpc_internal *rpc, const char *attribute, fastf_t sf, int rflag)
{
    RT_RPC_CK_MAGIC(rpc);

    switch (attribute[0]) {
	case 'b':
	case 'B':
	    if (!rflag)
		sf /= MAGNITUDE(rpc->rpc_B);

	    VSCALE(rpc->rpc_B, rpc->rpc_B, sf);
	    break;
	case 'h':
	case 'H':
	    if (!rflag)
		sf /= MAGNITUDE(rpc->rpc_H);

	    VSCALE(rpc->rpc_H, rpc->rpc_H, sf);
	    break;
	case 'r':
	case 'R':
	    if (rflag)
		rpc->rpc_r *= sf;
	    else
		rpc->rpc_r = sf;

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad rpc attribute - %s", attribute);
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
