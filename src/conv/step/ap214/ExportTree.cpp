/*                   A D D _ T R E E . C P P
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
/** @file step/ap214/ExportTree.cpp
 *
 * AP214 combination export fallback.
 */

#include "AP214e3.h"

#include "bu/ptbl.h"
#include "G_Objects.h"

STEPentity *
Comb_Tree_to_STEP(struct directory *dp, struct rt_wdb *wdbp, AP203_Contents *sc)
{
    /* The former experimental path constructed BOOLEAN_RESULT representation
     * items but assigned them to attributes requiring a REPRESENTATION.  A
     * single-leaf region was worse: it assigned MANIFOLD_SOLID_BREP directly.
     * Both forms are invalid AP214 and could crash a conforming re-import.
     *
     * A correct implementation needs a CSG_SHAPE_REPRESENTATION emitter in
     * the schema-specific layer.  Until then, retain all selected geometry as
     * unique advanced BReps.  This deliberately flattens combination boolean
     * and assembly semantics, but never claims success for malformed output. */
    const char *solid_search = "! -type comb";
    struct bu_ptbl solids = BU_PTBL_INIT_ZERO;
    (void)db_search(&solids, DB_SEARCH_RETURN_UNIQ_DP, solid_search, 1, &dp,
	wdbp->dbip, NULL, NULL, NULL);

    for (size_t i = 0; i < BU_PTBL_LEN(&solids); ++i) {
	struct directory *solid = (struct directory *)BU_PTBL_GET(&solids, i);
	if (sc->solid_to_step->find(solid) != sc->solid_to_step->end()) continue;

	struct rt_db_internal internal;
	if (rt_db_get_internal(&internal, solid, wdbp->dbip, bn_mat_identity) < 0)
	    continue;
	RT_CK_DB_INTERNAL(&internal);
	Object_To_STEP(solid, &internal, wdbp, sc);
	rt_db_free_internal(&internal);
    }
    db_search_free(&solids);
    return NULL;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
