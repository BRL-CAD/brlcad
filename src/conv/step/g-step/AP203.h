/*                            A P 2 0 3 . H
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
/** @file step/G_STEP_internal.h
 *
 */

#ifndef AP203_H
#define AP203_H

#include "common.h"

#include "raytrace.h"

#include "BRLCADWrapper.h"
#include "STEPWrapper.h"
#include "STEPfile.h"
#include "sdai.h"
#include "STEPcomplex.h"
#include "STEPattribute.h"

#include <sstream>
#include <map>
#include <set>


// Container structure that holds elements needed by multiple functions
struct AP203_Contents {
    Registry *registry;
    InstMgr *instance_list;
    STEPentity *default_context;
    SdaiApplication_context *application_context;
    SdaiDesign_context *design_context;
    std::map<struct directory *, STEPentity *> *solid_to_step;
    std::map<struct directory *, STEPentity *> *solid_to_step_shape;
    std::map<struct directory *, STEPentity *> *solid_to_step_manifold;
    std::map<struct directory *, STEPentity *> *comb_to_step;
    std::map<struct directory *, STEPentity *> *comb_to_step_shape;
    std::map<struct directory *, STEPentity *> *comb_to_step_manifold;
};



void XYZ_to_Cartesian_point(double x, double y, double z, SdaiCartesian_point *step_pnt);
void XYZ_to_Direction(double x, double y, double z, SdaiDirection *step_direction);

#endif /* AP203_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
