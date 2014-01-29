/*         S H A P E _ R E P R E S E N T A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file Shape_Representation.h
 *
 */

#ifndef SHAPE_REPRESENTATION_H
#define SHAPE_REPRESENTATION_H

#include "AP203.h"

SdaiRepresentation *
Add_Shape_Representation(Registry *registry, InstMgr *instance_list,
	SdaiRepresentation_context *context);

#endif /*SHAPE_REPRESENTATION_H*/

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
