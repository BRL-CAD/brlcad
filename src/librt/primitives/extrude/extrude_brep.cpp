/*                    E X T R U D E _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file extrude_brep.cpp
 *
 * Convert an Extruded Sketch to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "nmg.h"
#include "brep.h"

extern "C" {
    extern void rt_sketch_brep(ON_Brep **bi, const struct rt_db_internal *ip, const struct bn_tol *tol);
}


extern "C" void
rt_extrude_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_extrude_internal *eip;
    struct rt_db_internal tmp_internal;
    RT_DB_INTERNAL_INIT(&tmp_internal);

    eip = (struct rt_extrude_internal *)ip->idb_ptr;
    RT_EXTRUDE_CK_MAGIC(eip);

    // Create a sketch whose shape is according to eip->skt,
    // and position is according to eip->V, eip->u_vec and eip->v_vec.
    // Then convert the sketch to BREP.
    struct rt_sketch_internal sketch;
    sketch = *(eip->skt);
    VMOVE(sketch.V, eip->V);
    VMOVE(sketch.u_vec, eip->u_vec);
    VMOVE(sketch.v_vec, eip->v_vec);
    tmp_internal.idb_ptr = (void *)(&sketch);
    ON_Brep *extrude_brep = ON_Brep::New();
    rt_sketch_brep(&extrude_brep, &tmp_internal, tol);
    if (!extrude_brep || extrude_brep->m_F.Count() != 1) {
	delete extrude_brep;
	bu_log("rt_extrude_brep: could not construct a bounded sketch face\n");
	return;
    }

    // Create the extrude path and make the extrude primitive.
    vect_t endpoint;
    VADD2(endpoint, eip->V, eip->h);
    const ON_LineCurve extrude_path(ON_3dPoint(eip->V), ON_3dPoint(endpoint));
    if (ON_BrepExtrudeFace(*extrude_brep, 0, extrude_path, true) != 2) {
	delete extrude_brep;
	bu_log("rt_extrude_brep: openNURBS could not extrude and cap the face\n");
	return;
    }

    extrude_brep->Compact();
    extrude_brep->SetTolerancesBoxesAndFlags(false);
    const double model_tolerance = (tol && tol->dist > 0.0) ?
	tol->dist : RT_LEN_TOL;
    for (int i = 0; i < extrude_brep->m_E.Count(); ++i)
	if (extrude_brep->m_E[i].m_tolerance < 0.0)
	    extrude_brep->m_E[i].m_tolerance = model_tolerance;
    ON_wString messages;
    ON_TextLog log(messages);
    if (!extrude_brep->IsValid(&log) || !extrude_brep->IsSolid()) {
	ON_String text(messages);
	bu_log("rt_extrude_brep: generated BRep is not a valid manifold solid:\n%s",
	    text.Array());
	delete extrude_brep;
	return;
    }

    **b = *extrude_brep;
    delete extrude_brep;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
