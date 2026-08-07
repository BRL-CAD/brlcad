/*                   G _ O B J E C T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
#include "bu/log.h"
#include "ON_Brep.h"
#include "STEPBRepFallback.h"
#include "Shape_Definition_Representation.h"
#include "Trees.h"
#include "G_Objects.h"

namespace {

static bool
object_to_step(struct directory *dp, struct rt_db_internal *intern,
    struct rt_wdb *wdbp, AP203_Contents *sc, bool create_product,
    std::string *diagnostic)
{
    struct bn_tol tol;
    ON_Brep **brep;
    STEPentity *brep_shape = NULL;
    STEPentity *brep_product = NULL;
    STEPentity *brep_manifold = NULL;
    RT_CK_DB_INTERNAL(intern);
    std::map<struct directory *, STEPentity *>::iterator existing =
	sc->solid_to_step->find(dp);
    if (existing != sc->solid_to_step->end()) {
	if (!create_product)
	    return sc->solid_to_step_shape->find(dp) !=
		sc->solid_to_step_shape->end();
	if (!existing->second) {
	    const auto shape = sc->solid_to_step_shape->find(dp);
	    if (shape != sc->solid_to_step_shape->end())
		existing->second = Add_Shape_Definition_Representation(dp, sc,
		    shape->second);
	}
	return existing->second != NULL;
    }
    if (sc->comb_to_step->find(dp) != sc->comb_to_step->end()) return true;
    switch (intern->idb_minor_type) {
	case DB5_MINORTYPE_BRLCAD_BREP:
	    RT_BREP_CK_MAGIC((struct rt_brep_internal *)(intern->idb_ptr));
	    if (!((struct rt_brep_internal *)(intern->idb_ptr))->brep ||
		    !((struct rt_brep_internal *)(intern->idb_ptr))->brep->m_F.Count()) {
		bu_log("ERROR: BRep %s has no exportable faces\n", dp->d_namep);
		if (diagnostic) *diagnostic = "BRep has no exportable faces";
		return false;
	    }
	    if (!ON_BRep_to_STEP(dp,
		((struct rt_brep_internal *)(intern->idb_ptr))->brep, sc,
		&brep_shape, &brep_product, &brep_manifold, create_product,
		diagnostic))
		return false;
	    (*sc->solid_to_step)[dp] = brep_product;
	    (*sc->solid_to_step_shape)[dp] = brep_shape;
	    (*sc->solid_to_step_manifold)[dp] = brep_manifold;
	    return (!create_product || brep_product) && brep_shape &&
		brep_manifold;
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	    if (!create_product) return false;
	    (void)Comb_Tree_to_STEP(dp, wdbp, sc);
	    return sc->comb_to_step->find(dp) != sc->comb_to_step->end();
	default:
	    /* If it isn't already a BRep and it's not a comb, try to make it
	     * into a BRep */
	    ON_Brep *brep_obj = NULL;
	    if (intern->idb_meth->ft_brep != NULL) {
		tol.magic = BN_TOL_MAGIC;
		tol.dist = BN_TOL_DIST;
		tol.dist_sq = tol.dist * tol.dist;
		tol.perp = SMALL_FASTF;
		tol.para = 1.0 - tol.perp;
		brep_obj = brlcad::step::BRepFallback(intern, &tol);
		brep = &brep_obj;
		if (!(*brep)) {
		    bu_log("ERROR: failure to convert BRep %s (object type %s)\n",
			dp->d_namep, intern->idb_meth->ft_label);
		    if (diagnostic) *diagnostic = "BRL-CAD could not construct a BRep fallback";
		    return false;
		} else {
		    if (!ON_BRep_to_STEP(dp, *brep, sc, &brep_shape,
			    &brep_product, &brep_manifold, create_product,
			    diagnostic)) {
			delete brep_obj;
			return false;
		    }
		}
		(*sc->solid_to_step)[dp] = brep_product;
		(*sc->solid_to_step_shape)[dp] = brep_shape;
		(*sc->solid_to_step_manifold)[dp] = brep_manifold;
	    } else {
		bu_log("ERROR: no BRep representation is available for %s "
		    "(object type %s)\n", dp->d_namep, intern->idb_meth->ft_label);
		if (diagnostic) *diagnostic = "object has no BRep or schema-native representation";
		delete brep_obj;
		return false;
	    }
	    delete brep_obj;
	    return (!create_product || brep_product) && brep_shape &&
		brep_manifold;
    }
    return false;
}

} // namespace

bool
Object_To_STEP(struct directory *dp, struct rt_db_internal *intern,
    struct rt_wdb *wdbp, AP203_Contents *sc, std::string *diagnostic)
{
    return object_to_step(dp, intern, wdbp, sc, true, diagnostic);
}

bool
Object_Geometry_To_STEP(struct directory *dp, struct rt_db_internal *intern,
    struct rt_wdb *wdbp, AP203_Contents *sc, std::string *diagnostic)
{
    return object_to_step(dp, intern, wdbp, sc, false, diagnostic);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
