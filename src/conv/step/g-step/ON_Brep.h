/*                 ON_Brep.h
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file step/ON_Brep.h
 *
 */

#ifndef ON_BREP_H_
#define ON_BREP_H_

#include "STEPWrapper.h"

struct Exporter_Info_AP203 {
    Registry *registry;
    InstMgr *instance_list;
    std::vector<STEPentity *> cartesian_pnts;
    std::vector<STEPentity *> vertex_pnts;
    std::vector<STEPentity *> three_dimensional_curves;
    std::vector<STEPentity *> edge_curves;
    std::vector<STEPentity *> oriented_edges;
    std::vector<STEPentity *> edge_loops;
    std::vector<STEPentity *> outer_bounds;
    std::vector<STEPentity *> surfaces;
    std::vector<STEPentity *> faces;
    SdaiClosed_shell *closed_shell;
    SdaiManifold_solid_brep *manifold_solid_brep;
    SdaiAdvanced_brep_shape_representation *advanced_brep;
    SdaiRepresentation *shape_rep;

    std::map<int, std::pair<STEPentity *, STEPentity *> > sdai_curve_to_splits;
    std::map<int, STEPentity * > split_midpt_vertex;
};

bool ON_BRep_to_STEP(ON_Brep *brep, Exporter_Info_AP203 *info);

#endif /* ON_BREP_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
