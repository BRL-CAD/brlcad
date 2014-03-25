/*                 ON_Brep.h
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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

#ifndef ON_BREP_H
#define ON_BREP_H

#include "AP_Common.h"

struct ON_Brep_Info_AP203 {
    Registry *registry;
    InstMgr *instance_list;
    int split_closed;
    std::vector<STEPentity *> cartesian_pnts;
    std::vector<STEPentity *> vertex_pnts;
    std::vector<STEPentity *> vectors;
    std::vector<STEPentity *> directions;
    std::vector<STEPentity *> three_dimensional_curves;
    std::vector<STEPentity *> edge_curves;
    std::vector<STEPentity *> oriented_edges;
    std::vector<STEPentity *> edge_loops;
    std::vector<STEPentity *> inner_bounds;
    std::vector<STEPentity *> outer_bounds;
    std::vector<STEPentity *> surfaces;
    std::vector<STEPentity *> faces;
    SdaiClosed_shell *closed_shell;
    SdaiManifold_solid_brep *manifold_solid_brep;
    SdaiAdvanced_brep_shape_representation *advanced_brep;

    std::map<STEPentity*, GenericAggregate * > surf_genagg;
    std::map<STEPentity*, std::vector<std::vector<STEPentity *> > > surface_cv;
};

void ON_3dPoint_to_Cartesian_point(ON_3dPoint *inpnt, SdaiCartesian_point *step_pnt);
void ON_3dVector_to_Direction(ON_3dVector *invect, SdaiDirection *step_direction);

bool ON_NurbsCurve_to_STEP(ON_NurbsCurve *n_curve, ON_Brep_Info_AP203 *info, int i);
void ON_NurbsSurfaceCV_Finalize_GenericAggregates(ON_Brep_Info_AP203 *info);
bool ON_NurbsSurface_to_STEP(ON_NurbsSurface *n_surface, ON_Brep_Info_AP203 *info, int i);

void ON_BRep_to_STEP(struct directory *dp, ON_Brep *brep, AP203_Contents *sc,
        STEPentity **brep_shape, STEPentity **brep_product, STEPentity **brep_manifold);

#endif /* ON_BREP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
