/*                 S C A L E _  M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/scale_metaball.c
 *
 * The scale_metaball command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../ged_private.h"
#include "./ged_edit.h"

#define GED_METABALL_SCALE(_d, _scale) \
    if ((_scale) < 0.0) \
	(_d) = -(_scale); \
    else		  \
	(_d) *= (_scale);

int
_ged_scale_metaball(struct ged *gedp, struct rt_metaball_internal *mbip, const char *attribute, fastf_t sf, int rflag)
{
    int mbp_i;
    struct wdb_metaball_pnt *mbpp;

    RT_METABALL_CK_MAGIC(mbip);

    if (!rflag && sf > 0)
	sf = -sf;

    switch (attribute[0]) {
	case 'f':
	case 'F':
	    if (sscanf(attribute+1, "%d", &mbp_i) != 1)
		mbp_i = 0;

	    if ((mbpp = rt_metaball_get_pt_i(mbip, mbp_i)) == (struct wdb_metaball_pnt *)NULL)
		return BRLCAD_ERROR;

	    BU_CKMAG(mbpp, WDB_METABALLPT_MAGIC, "wdb_metaball_pnt");
	    GED_METABALL_SCALE(mbpp->fldstr, sf);

	    break;
	case 's':
	case 'S':
	    if (sscanf(attribute+1, "%d", &mbp_i) != 1)
		mbp_i = 0;

	    if ((mbpp = rt_metaball_get_pt_i(mbip, mbp_i)) == (struct wdb_metaball_pnt *)NULL)
		return BRLCAD_ERROR;

	    BU_CKMAG(mbpp, WDB_METABALLPT_MAGIC, "wdb_metaball_pnt");
	    GED_METABALL_SCALE(mbpp->sweat, sf);

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad metaball attribute - %s", attribute);
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
