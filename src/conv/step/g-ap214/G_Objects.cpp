/*                   G _ O B J E C T S . C P P
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
/** @file G_Objects.cpp
 *
 * Toplevel routine for writing out BRL-CAD geometry objects
 * into the STEPcode containers
 *
 */

#include "AP_Common.h"
#include "bu.h"
#include "ON_Brep.h"
#include "Trees.h"
#include "G_Objects.h"

void
Object_To_STEP(struct directory *dp, struct rt_db_internal *intern, struct rt_wdb *wdbp, AP203_Contents *sc)
{
    int type;
    struct bn_tol tol;
    struct bn_tol arb_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
    ON_Brep **brep;
    STEPentity *brep_shape;
    STEPentity *brep_product;
    RT_CK_DB_INTERNAL(intern);
    if (sc->solid_to_step->find(dp) != sc->solid_to_step->end()) return;
    if (sc->comb_to_step->find(dp) != sc->comb_to_step->end()) return;
    switch (intern->idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_HALF: /* half_space_solid */
	    bu_log("half");
	    //break;
	case DB5_MINORTYPE_BRLCAD_SPH: /* sphere */
	    bu_log("sph");
	    //break;
	case DB5_MINORTYPE_BRLCAD_ELL:
	    /* Check if the ELL happens to be a sphere */
	    bu_log("sph");
	    //break; - NOTE that we only want to break here if the ell is actually a sphere- otherwise we fall through
	case DB5_MINORTYPE_BRLCAD_REC:
	    /* Check if the REC happens to be an RCC - right circular cylinder */
	    bu_log("rec");
	    //break; - NOTE that we only want to break here if the rec is actually a rcc - otherwise we fall through
	case DB5_MINORTYPE_BRLCAD_TGC:
	    /* Check if the TGC happens to be an RCC - right circular cylinder,
	       -	     * or if it can be approximated as a right circular cone */
	    bu_log("tgc");
	    //break; - NOTE that we only want to break here if the tgc is actually a rcc - otherwise we fall through
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    /* In principle, some arbs will satisfy block and
	       -	     * right_angular_wedge - not clear if it's worth
	       -	     * testing the CSG inputs to identify them */
	    type = rt_arb_std_type(intern, &arb_tol);
	    switch (type) {
		case 4:
		    bu_log("arb4");
		    //break;
		case 5:
		    bu_log("arb5");
		    //break;
		case 6:
		    bu_log("arb6");
		    //break;
		case 7:
		    bu_log("arb7");
		    //break;
		case 8:
		    bu_log("arb8");
		    //break;
		default:
		    bu_log("invalid");
		    //break;
	    }
	    //break;
	case DB5_MINORTYPE_BRLCAD_TOR:
	    /* The torus, while expressible in AP214 as a geometric representation,
	       -	 * do not appear to be regarded as a csg_primitive or solid.  The
	       -	 * toroidal surface might be used to construct a solid, but it's not
	       -	 * clear at this point that there is any advantage over just using the
	       -	 * NURBS form. */
	    bu_log("tor");
	    //break;
	case DB5_MINORTYPE_BRLCAD_BREP:
	    RT_BREP_TEST_MAGIC((struct rt_brep_internal *)(intern->idb_ptr));
	    (void)ON_BRep_to_STEP(dp, ((struct rt_brep_internal *)(intern->idb_ptr))->brep, sc, &brep_shape, &brep_product);
	    (*sc->solid_to_step)[dp] = brep_product;
	    (*sc->solid_to_step_shape)[dp] = brep_shape;
	    break;
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	    //(void)Comb_Tree_to_STEP(dp, wdbp, sc);
	    break;
	default:
	    /* If it isn't already a BRep, it's not a comb, and it's not a special case try to make it
	     * into a BRep */
	    ON_Brep *brep_obj = ON_Brep::New();
	    if (intern->idb_meth->ft_brep != NULL) {
		tol.magic = BN_TOL_MAGIC;
		tol.dist = BN_TOL_DIST;
		tol.dist_sq = tol.dist * tol.dist;
		tol.perp = SMALL_FASTF;
		tol.para = 1.0 - tol.perp;
		brep = &brep_obj;
		intern->idb_meth->ft_brep(brep, intern, &tol);
		if (!(*brep)) {
		    bu_log("failure to convert brep %s (object type %s)\n", dp->d_namep, intern->idb_meth->ft_label);
		    ON_BRep_to_STEP(dp, NULL, sc, &brep_shape, &brep_product);
		} else {
		    ON_BRep_to_STEP(dp, *brep, sc, &brep_shape, &brep_product);
		}
		(*sc->solid_to_step)[dp] = brep_product;
		(*sc->solid_to_step_shape)[dp] = brep_shape;
	    } else {
		/* Out of luck */
		ON_BRep_to_STEP(dp, NULL, sc, &brep_shape, &brep_product);
		(*sc->solid_to_step)[dp] = brep_product;
		(*sc->solid_to_step_shape)[dp] = brep_shape;
		bu_log("WARNING: No Brep representation available (object type %s) - object %s will be empty in the STEP output.\n", intern->idb_meth->ft_label, dp->d_namep);
	    }
	    delete brep_obj;
	    break;
    }
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
