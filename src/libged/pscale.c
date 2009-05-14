/*                         P S C A L E . C
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
ged_pscale(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    struct rt_db_internal intern;
    fastf_t sf;
    mat_t mat;
    char *last;
    struct directory *dp;
    static const char *usage = "obj attribute sf";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "bad attribute - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &sf) != 1 ||
	sf <= SQRT_SMALL_FASTF) {
	bu_vls_printf(&gedp->ged_result_str, "bad scale factor - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(&gedp->ged_result_str, "illegal input - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s not found", argv[1]);
	return BRLCAD_ERROR;
    }

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	bu_vls_printf(&gedp->ged_result_str, "Object not eligible for scaling.");
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_ERROR;
    }

    switch (intern.idb_minor_type) {
    case DB5_MINORTYPE_BRLCAD_EHY:
	ret = ged_scale_ehy(gedp, (struct rt_ehy_internal *)intern.idb_ptr, argv[2], sf);
	break;
    case DB5_MINORTYPE_BRLCAD_ELL:
	ret = ged_scale_ell(gedp, (struct rt_ell_internal *)intern.idb_ptr, argv[2], sf);
	break;
    case DB5_MINORTYPE_BRLCAD_EPA:
	ret = ged_scale_epa(gedp, (struct rt_epa_internal *)intern.idb_ptr, argv[2], sf);
	break;
    case DB5_MINORTYPE_BRLCAD_PARTICLE:
	ret = ged_scale_part(gedp, (struct rt_part_internal *)intern.idb_ptr, argv[2], sf);
	break;
    case DB5_MINORTYPE_BRLCAD_TOR:
	ret = ged_scale_tor(gedp, (struct rt_tor_internal *)intern.idb_ptr, argv[2], sf);
	break;
    default:
	bu_vls_printf(&gedp->ged_result_str, "Object not yet supported.");
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_ERROR;
    }

    if (ret == BRLCAD_OK) {
	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, BRLCAD_ERROR);
    } else if (ret == BRLCAD_ERROR) {
	rt_db_free_internal(&intern, &rt_uniresource);
    }

    return ret;
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
