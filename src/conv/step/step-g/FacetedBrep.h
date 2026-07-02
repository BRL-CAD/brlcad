/*                 FacetedBrep.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/FacetedBrep.h
 *
 * Class definition used to convert STEP "FacetedBrep" to BRL-CAD
 * structures.
 *
 * A faceted_brep is a manifold_solid_brep all of whose faces are planar
 * and bounded by poly_loops (ordered vertex polygons).  Because it is a
 * pure polyhedral mesh it is imported as a BRL-CAD BoT (bag of triangles)
 * rather than as an OpenNURBS BRep.
 *
 */

#ifndef CONV_STEP_STEP_G_FACETEDBREP_H
#define CONV_STEP_STEP_G_FACETEDBREP_H

#include "common.h"

#include <vector>

#include "vmath.h"

#include "ManifoldSolidBrep.h"

// forward declaration of class
class STEPWrapper;

class FacetedBrep: public ManifoldSolidBrep
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    FacetedBrep();
    FacetedBrep(STEPWrapper *sw, int step_id);
    virtual ~FacetedBrep();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual void Print(int level);

    /* Walk the outer shell's planar poly_loop faces and produce an indexed
     * triangle mesh.  'vertices' is filled with flat x,y,z triples (already
     * scaled to mm by LocalUnits::length) and 'faces' with flat vertex-index
     * triples.  Returns true if at least one triangle was produced.
     */
    bool GetBoT(std::vector<fastf_t> &vertices, std::vector<int> &faces);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_FACETEDBREP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
