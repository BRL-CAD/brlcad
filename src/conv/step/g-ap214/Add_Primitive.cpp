/*               A D D _ P R I M I T I V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file ON_Brep.cpp
 *
 * File for writing out BRL-CAD primitives into the STEPcode containers
 *
 */

/* In a few cases, AP214 provides a csg_primitive that corresponds to
 * one of BRL-CAD's primitives.  Most of the time, this is not the case
 * and we have to fall back on the NURBS BRep form.*/

#include "ON_Brep.h"
#include "AP214.h"

struct AP214_primitive_result *
AP214_Add_Primitive(struct directory *dp, struct rt_wdb *wdbp, Registry *registry, InstMgr *instance_list)
{
    struct rt_db_internal intern;
    struct db_i *dbip = wdbp->dbip;
    rt_db_get_internal(&intern, dp, dbip, bn_mat_identity, &rt_uniresource);
    RT_CK_DB_INTERNAL(&intern);

    struct AP214_primitive_result *result = new struct AP214_primitive_result;

    switch (intern.idb_minor_type) {
#if 0
	case DB5_MINORTYPE_BRLCAD_HALF: /* half_space_solid */
	    bu_vls_printf(gedp->ged_result_str, "half");
	    break;
	case DB5_MINORTYPE_BRLCAD_SPH: /* sphere */
	    bu_vls_printf(gedp->ged_result_str, "sph");
	    break;
	case DB5_MINORTYPE_BRLCAD_ELL:
	    /* Check if the ELL happens to be a sphere */
	    bu_vls_printf(gedp->ged_result_str, "sph");
	    break;
	case DB5_MINORTYPE_BRLCAD_REC:
	    /* Check if the REC happens to be an RCC - right circular cylinder */
	    bu_vls_printf(gedp->ged_result_str, "rec");
	    break;
	case DB5_MINORTYPE_BRLCAD_TGC:
	    /* Check if the TGC happens to be an RCC - right circular cylinder,
	     * or if it can be approximated as a right circular cone */
	    bu_vls_printf(gedp->ged_result_str, "tgc");
	    break;
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    /* In principle, some arbs will satisfy block and
	     * right_angular_wedge - not clear if it's worth
	     * testing the CSG inputs to identify them */
	    struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
	    type = rt_arb_std_type(&intern, &arb_tol);
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

	/* The torus, while expressible in AP214 as a geometric representation,
	 * do not appear to be regarded as a csg_primitive or solid.  The
	 * toroidal surface might be used to construct a solid, but it's not
	 * clear at this point that there is any advantage over just using the
	 * NURBS form. */
	case DB5_MINORTYPE_BRLCAD_TOR:
	    bu_vls_printf(gedp->ged_result_str, "tor");
	    break;
#endif
	/* From here on, we have to use the NURBS BRep form */
	default:
	    if (intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_BREP) {
		ON_BRep_to_STEP(dp, ((struct rt_brep_internal *)(intern.idb_ptr))->brep, registry, instance_list, &result->shape, &result->product);
	    } else {
		if (intern.idb_meth->ft_brep != NULL) {
		    struct bn_tol tol;
		    tol.magic = BN_TOL_MAGIC;
		    tol.dist = BN_TOL_DIST;
		    tol.dist_sq = tol.dist * tol.dist;
		    tol.perp = SMALL_FASTF;
		    tol.para = 1.0 - tol.perp;
		    ON_Brep *brep_obj = ON_Brep::New();
		    ON_Brep **brep = &brep_obj;
		    intern.idb_meth->ft_brep(brep, &intern, &tol);
		    ON_BRep_to_STEP(dp, *brep, registry, instance_list, &result->shape, &result->product);
		} else {
		    bu_log("Unsupported primitive type: %s\n", intern.idb_meth->ft_label);
		}
	    }
	    break;
    }

    return result;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
