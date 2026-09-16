/* S H A P E _ R E P R E S E N T A T I O N _ R E L A T I O N S H I P . H
 *
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
/** @file Shape_Representation_Relationship.h
 *
 */

#ifndef SHAPE_REPRESENTATION_RELATIONSHIP_H
#define SHAPE_REPRESENTATION_RELATIONSHIP_H

#include "AP_Common.h"

/* Assign a representation-valued relationship attribute through STEPcode's
 * public conversion API.  The physical attribute is an entity reference in
 * AP203/AP214/AP242e1 and a SELECT in AP242e2 and later. */
bool
Set_Representation_Relationship_Reference(STEPentity *relationship,
	const char *attribute_name, STEPentity *representation,
	InstMgr *instances);

/* Shape Representation Relationship
 */
STEPentity *
Add_Shape_Representation_Relationship(AP203_Contents *sc,
	STEPentity *shape_rep, STEPentity *manifold_shape);

#endif /*SHAPE_REPRESENTATION_RELATIONSHIP_H*/

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
