/*                         G E T _ T Y P E . C
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
/** @file libged/get_type.c
 *
 * The get_type command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_get_type(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    int type;
    static const char *usage = "object";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (wdb_import_from_path(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	bu_vls_printf(gedp->ged_result_str, "unknown");
	rt_db_free_internal(&intern);

	return GED_OK;
    }

    switch (intern.idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_TOR:
	    bu_vls_printf(gedp->ged_result_str, "tor");
	    break;
	case DB5_MINORTYPE_BRLCAD_TGC:
	    bu_vls_printf(gedp->ged_result_str, "tgc");
	    break;
	case DB5_MINORTYPE_BRLCAD_ELL:
	    bu_vls_printf(gedp->ged_result_str, "ell");
	    break;
	case DB5_MINORTYPE_BRLCAD_SUPERELL:
	    bu_vls_printf(gedp->ged_result_str, "superell");
	    break;
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);

	    switch (type) {
		case 4:
		    bu_vls_printf(gedp->ged_result_str, "arb4");
		    break;
		case 5:
		    bu_vls_printf(gedp->ged_result_str, "arb5");
		    break;
		case 6:
		    bu_vls_printf(gedp->ged_result_str, "arb6");
		    break;
		case 7:
		    bu_vls_printf(gedp->ged_result_str, "arb7");
		    break;
		case 8:
		    bu_vls_printf(gedp->ged_result_str, "arb8");
		    break;
		default:
		    bu_vls_printf(gedp->ged_result_str, "invalid");
		    break;
	    }

	    break;
	case DB5_MINORTYPE_BRLCAD_ARS:
	    bu_vls_printf(gedp->ged_result_str, "ars");
	    break;
	case DB5_MINORTYPE_BRLCAD_HALF:
	    bu_vls_printf(gedp->ged_result_str, "half");
	    break;
	case DB5_MINORTYPE_BRLCAD_HYP:
	    bu_vls_printf(gedp->ged_result_str, "hyp");
	    break;
	case DB5_MINORTYPE_BRLCAD_REC:
	    bu_vls_printf(gedp->ged_result_str, "rec");
	    break;
	case DB5_MINORTYPE_BRLCAD_POLY:
	    bu_vls_printf(gedp->ged_result_str, "poly");
	    break;
	case DB5_MINORTYPE_BRLCAD_BSPLINE:
	    bu_vls_printf(gedp->ged_result_str, "spline");
	    break;
	case DB5_MINORTYPE_BRLCAD_SPH:
	    bu_vls_printf(gedp->ged_result_str, "sph");
	    break;
	case DB5_MINORTYPE_BRLCAD_NMG:
	    bu_vls_printf(gedp->ged_result_str, "nmg");
	    break;
	case DB5_MINORTYPE_BRLCAD_EBM:
	    bu_vls_printf(gedp->ged_result_str, "ebm");
	    break;
	case DB5_MINORTYPE_BRLCAD_VOL:
	    bu_vls_printf(gedp->ged_result_str, "vol");
	    break;
	case DB5_MINORTYPE_BRLCAD_ARBN:
	    bu_vls_printf(gedp->ged_result_str, "arbn");
	    break;
	case DB5_MINORTYPE_BRLCAD_PIPE:
	    bu_vls_printf(gedp->ged_result_str, "pipe");
	    break;
	case DB5_MINORTYPE_BRLCAD_PARTICLE:
	    bu_vls_printf(gedp->ged_result_str, "part");
	    break;
	case DB5_MINORTYPE_BRLCAD_RPC:
	    bu_vls_printf(gedp->ged_result_str, "rpc");
	    break;
	case DB5_MINORTYPE_BRLCAD_RHC:
	    bu_vls_printf(gedp->ged_result_str, "rhc");
	    break;
	case DB5_MINORTYPE_BRLCAD_EPA:
	    bu_vls_printf(gedp->ged_result_str, "epa");
	    break;
	case DB5_MINORTYPE_BRLCAD_EHY:
	    bu_vls_printf(gedp->ged_result_str, "ehy");
	    break;
	case DB5_MINORTYPE_BRLCAD_ETO:
	    bu_vls_printf(gedp->ged_result_str, "eto");
	    break;
	case DB5_MINORTYPE_BRLCAD_GRIP:
	    bu_vls_printf(gedp->ged_result_str, "grip");
	    break;
	case DB5_MINORTYPE_BRLCAD_JOINT:
	    bu_vls_printf(gedp->ged_result_str, "joint");
	    break;
	case DB5_MINORTYPE_BRLCAD_HF:
	    bu_vls_printf(gedp->ged_result_str, "hf");
	    break;
	case DB5_MINORTYPE_BRLCAD_DSP:
	    bu_vls_printf(gedp->ged_result_str, "dsp");
	    break;
	case DB5_MINORTYPE_BRLCAD_SKETCH:
	    bu_vls_printf(gedp->ged_result_str, "sketch");
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    bu_vls_printf(gedp->ged_result_str, "extrude");
	    break;
	case DB5_MINORTYPE_BRLCAD_SUBMODEL:
	    bu_vls_printf(gedp->ged_result_str, "submodel");
	    break;
	case DB5_MINORTYPE_BRLCAD_CLINE:
	    bu_vls_printf(gedp->ged_result_str, "cline");
	    break;
	case DB5_MINORTYPE_BRLCAD_BOT:
	    bu_vls_printf(gedp->ged_result_str, "bot");
	    break;
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	    bu_vls_printf(gedp->ged_result_str, "comb");
	    break;
	case DB5_MINORTYPE_BRLCAD_BREP:
	    bu_vls_printf(gedp->ged_result_str, "brep");
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "other");
	    break;
    }

    rt_db_free_internal(&intern);
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
