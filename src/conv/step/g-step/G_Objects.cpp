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
    struct bn_tol tol;
    ON_Brep **brep;
    STEPentity *brep_shape;
    STEPentity *brep_product;
    STEPentity *brep_manifold;
    RT_CK_DB_INTERNAL(intern);
    if (sc->solid_to_step->find(dp) != sc->solid_to_step->end()) return;
    if (sc->comb_to_step->find(dp) != sc->comb_to_step->end()) return;
    switch (intern->idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_BREP:
	    RT_BREP_TEST_MAGIC((struct rt_brep_internal *)(intern->idb_ptr));
	    (void)ON_BRep_to_STEP(dp, ((struct rt_brep_internal *)(intern->idb_ptr))->brep, sc, &brep_shape, &brep_product, &brep_manifold);
	    (*sc->solid_to_step)[dp] = brep_product;
	    (*sc->solid_to_step_shape)[dp] = brep_shape;
	    (*sc->solid_to_step_manifold)[dp] = brep_manifold;
	    break;
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	    (void)Comb_Tree_to_STEP(dp, wdbp, sc);
	    break;
	default:
	    /* If it isn't already a BRep and it's not a comb, try to make it
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
		    ON_BRep_to_STEP(dp, NULL, sc, &brep_shape, &brep_product, &brep_manifold);
		} else {
		    ON_BRep_to_STEP(dp, *brep, sc, &brep_shape, &brep_product, &brep_manifold);
		}
		(*sc->solid_to_step)[dp] = brep_product;
		(*sc->solid_to_step_shape)[dp] = brep_shape;
		(*sc->solid_to_step_manifold)[dp] = brep_manifold;
	    } else {
		/* Out of luck */
		ON_BRep_to_STEP(dp, NULL, sc, &brep_shape, &brep_product, &brep_manifold);
		(*sc->solid_to_step)[dp] = brep_product;
		(*sc->solid_to_step_shape)[dp] = brep_shape;
		(*sc->solid_to_step_manifold)[dp] = brep_manifold;
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
