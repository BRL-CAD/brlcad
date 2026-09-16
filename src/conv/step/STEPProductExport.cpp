/* S H A P E _ D E F I N I T I O N _ R E P R E S E N T A T I O N . C P P
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
/** @file step/STEPProductExport.cpp
 *
 * File for writing out a Shape Definition Representation
 *
 */

#include "AP_Common.h"
#include "STEPGeneratedAPI.h"
#include "Shape_Definition_Representation.h"

/* Shape Definition Representation
 *
 */
STEPentity *
Add_Shape_Definition_Representation(struct directory *dp, AP203_Contents *sc, STEPentity *sdairep)
{
    std::ostringstream ss;
    ss << "'" << dp->d_namep << "'";
    std::string str = ss.str();

    // SHAPE_DEFINITION_REPRESENTATION
    STEPentity *shape_def_rep = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "SHAPE_DEFINITION_REPRESENTATION");
    brlcad::step::SetEntity(shape_def_rep, "used_representation", sdairep);

    // PRODUCT_DEFINITION_SHAPE
    STEPentity *prod_def_shape = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "PRODUCT_DEFINITION_SHAPE");
    brlcad::step::SetString(prod_def_shape, "name", "''");
    brlcad::step::SetString(prod_def_shape, "description", "''");
    brlcad::step::SetEntity(shape_def_rep, "definition", prod_def_shape);

    // PRODUCT_DEFINITION
    STEPentity *prod_def = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "PRODUCT_DEFINITION");
    brlcad::step::SetEntity(prod_def_shape, "definition", prod_def);
    brlcad::step::SetString(prod_def, "id", "''");
    brlcad::step::SetString(prod_def, "description", "''");

    // PRODUCT_DEFINITION_FORMATION
    STEPentity *prod_def_form = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "PRODUCT_DEFINITION_FORMATION");
    brlcad::step::SetEntity(prod_def, "formation", prod_def_form);
    brlcad::step::SetString(prod_def_form, "id", "''");
    brlcad::step::SetString(prod_def_form, "description", "''");

    // PRODUCT
    STEPentity *prod = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "PRODUCT");
    brlcad::step::SetEntity(prod_def_form, "of_product", prod);
    brlcad::step::SetString(prod, "name", str.c_str());
    brlcad::step::SetString(prod, "description", "''");
    brlcad::step::SetString(prod, "id", str.c_str());

    // PRODUCT_DEFINITION_CONTEXT
    STEPentity *prod_def_context = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "PRODUCT_DEFINITION_CONTEXT");
    brlcad::step::SetEntity(prod_def, "frame_of_reference", prod_def_context);
    brlcad::step::SetString(prod_def_context, "life_cycle_stage", "'part definition'");

    // PRODUCT_CONTEXT
    STEPentity *product_context = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "PRODUCT_CONTEXT");
    brlcad::step::SetString(product_context, "name", "''");

    EntityAggregate *context_agg = dynamic_cast<EntityAggregate *>(
	brlcad::step::Aggregate(prod, "frame_of_reference"));
    if (context_agg)
	context_agg->AddNode(new EntityNode(product_context));

    // APPLICATION_CONTEXT
    brlcad::step::SetEntity(prod_def_context, "frame_of_reference",
	sc->application_context);
    brlcad::step::SetEntity(product_context, "frame_of_reference",
	sc->application_context);

    //return ret_entity;
    // The product definition is what is used to define assemblies, so return that
    return prod_def;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
