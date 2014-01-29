/*               G _ S T E P _ I N T E R N A L . H
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

#ifndef CONV_STEP_G_STEP_G_STEP_INTERNAL_H
#define CONV_STEP_G_STEP_G_STEP_INTERNAL_H

#include "STEPWrapper.h"

void XYZ_to_Cartesian_point(double x, double y, double z, SdaiCartesian_point *step_pnt);
void XYZ_to_Direction(double x, double y, double z, SdaiDirection *step_direction);

STEPentity * Add_Shape_Representation_Relationship(Registry *registry, InstMgr *instance_list, SdaiRepresentation *shape_rep, SdaiRepresentation *manifold_shape);
SdaiRepresentation * Add_Shape_Representation(Registry *registry, InstMgr *instance_list, SdaiRepresentation_context *context);
STEPentity * Add_Shape_Definition_Representation(Registry *registry, InstMgr *instance_list, SdaiRepresentation *sdairep);
STEPcomplex * Add_Default_Geometric_Context(Registry *registry, InstMgr *instance_list);

#endif /* CONV_STEP_G_STEP_G_STEP_INTERNAL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
