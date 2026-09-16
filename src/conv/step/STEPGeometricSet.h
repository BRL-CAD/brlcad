/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
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
 *
 * Schema-neutral GEOMETRIC_SET import interfaces.
 */

#ifndef CONV_STEP_STEPGEOMETRICSET_H
#define CONV_STEP_STEPGEOMETRICSET_H

#include <string>

#include "STEPConversionStatus.h"
#include "STEPDocument.h"

class BRLCADWrapper;
class GeometricSet;
class Plane;
class Representation;
class STEPWrapper;

bool step_geometric_set_has_curves(GeometricSet *set);
bool step_geometric_set_has_points(GeometricSet *set);
bool step_geometric_set_has_surfaces(GeometricSet *set);

BrepWriteStatus step_convert_geometric_set(GeometricSet *set,
    Representation *representation, STEPWrapper *wrapper,
    BRLCADWrapper *database, std::string *name, int dry_run,
    const brlcad::step::Style *style_override = NULL);

BrepWriteStatus step_convert_datum_plane(Plane *plane,
    Representation *representation, BRLCADWrapper *database,
    const std::string &name, int dry_run);

#endif /* CONV_STEP_STEPGEOMETRICSET_H */
